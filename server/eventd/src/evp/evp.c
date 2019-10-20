/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2021 Quentin "Sardem FF7" Glidic
 *
 * This file is part of eventd.
 *
 * eventd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * eventd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with eventd. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "libeventd-event.h"
#include "eventd-plugin.h"
#include "libeventd-helpers-config.h"

#include "../eventd.h"
#include "../sd-modules.h"
#include "../plugins.h"
#include "client.h"

#include "evp.h"
#include "evp-internal.h"


static const gchar * const _eventd_evp_default_binds[] = {
#ifdef G_OS_UNIX
    "unix-runtime:" EVP_UNIX_SOCKET,
#else /* ! G_OS_UNIX */
    "tcp-file-runtime:" EVP_UNIX_SOCKET,
#endif /* ! G_OS_UNIX */
    NULL
};

/*
 * Initialization interface
 */

EventdEvpContext *
eventd_evp_init(EventdCoreContext *core, const gchar * const *binds, GList **used_sockets)
{
    EventdEvpContext *self;

    self = g_new0(EventdEvpContext, 1);

    self->core = core;

    self->ws = eventd_ws_init();

    self->service = g_socket_service_new();
    g_socket_service_stop(self->service);
    g_signal_connect(self->service, "incoming", G_CALLBACK(eventd_evp_client_connection_handler), self);

    if ( binds == NULL )
        binds = _eventd_evp_default_binds;

    GList *sockets;
    sockets = eventd_core_get_binds(self->core, binds);

    GList *socket_, *next_;
    for ( socket_ = sockets ; socket_ != NULL ; socket_ = next_ )
    {
        GSocket *socket = socket_->data;
        GError *error = NULL;
        next_ = g_list_next(socket_);

        if ( ! g_socket_listener_add_socket(G_SOCKET_LISTENER(self->service), socket, NULL, &error) )
        {
            g_warning("Unable to add socket: %s", error->message);
            g_clear_error(&error);
            sockets = g_list_delete_link(sockets, socket_);
            g_object_unref(socket);
        }
    }
    *used_sockets = sockets;

    return self;
}

void
eventd_evp_uninit(EventdEvpContext *self)
{
    g_socket_listener_close(G_SOCKET_LISTENER(self->service));
    g_object_unref(self->service);

    eventd_ws_uninit(self->ws);

    g_free(self);
}


/*
 * Start/Stop interface
 */

void
eventd_evp_start(EventdEvpContext *self)
{
    self->subscribe_categories = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_list_free);
    g_socket_service_start(self->service);
}

void
eventd_evp_stop(EventdEvpContext *self)
{
    g_hash_table_unref(self->subscribe_categories);
    self->subscribe_categories = NULL;
    g_list_free(self->subscribe_all);
    self->subscribe_all = NULL;

    g_list_free_full(self->clients, eventd_evp_client_disconnect);
    self->clients = NULL;

    g_socket_service_stop(self->service);
}


/*
 * Configuration interface
 */

static gboolean
_eventd_evp_load_certificate(EventdEvpContext *self, const gchar *cert_file, const gchar *key_file)
{
    GError *error = NULL;
    GTlsCertificate *cert;
    if ( key_file != NULL )
        cert = g_tls_certificate_new_from_files(cert_file, key_file, &error);
    else
        cert = g_tls_certificate_new_from_file(cert_file, &error);

    if ( cert != NULL )
    {
        if ( self->certificate != NULL )
            g_object_unref(self->certificate);
        self->certificate = cert;
        eventd_plugins_relay_set_certificate(self->certificate);
        return TRUE;
    }

    if ( key_file != NULL )
        g_warning("Could not read certificate (%s) or key (%s) file: %s", cert_file, key_file, error->message);
    else
        g_warning("Could not read certificate file (%s): %s", cert_file, error->message);
    g_clear_error(&error);
    return FALSE;
}

static gboolean
_eventd_evp_load_client_certificates(EventdEvpContext *self, const gchar *client_certs_file)
{
    GError *error = NULL;
    GList *certs;

    certs = g_tls_certificate_list_new_from_file(client_certs_file, &error);

    if ( error != NULL )
    {
        g_warning("Could not read client certificates file (%s): %s", client_certs_file, error->message);
        g_clear_error(&error);
        return FALSE;
    }
    g_list_free_full(self->client_certificates, g_object_unref);
    self->client_certificates = certs;
    return TRUE;
}

static void
_eventd_evp_cert_key_file_changed(EventdEvpContext *self, GFile *file, GFile *other_file, GFileMonitorEvent event_type, GFileMonitor *monitor)
{
    switch ( event_type )
    {
    case G_FILE_MONITOR_EVENT_CHANGED:
    break;
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
        g_debug("Reload certificate file");
        _eventd_evp_load_certificate(self, self->cert_file, self->key_file);
    break;
    default:
    break;
    }
}

static void
_eventd_evp_client_certs_file_changed(EventdEvpContext *self, GFile *file, GFile *other_file, GFileMonitorEvent event_type, GFileMonitor *monitor)
{
    switch ( event_type )
    {
    case G_FILE_MONITOR_EVENT_CHANGED:
    break;
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
        g_debug("Reload client certificates file");
        _eventd_evp_load_client_certificates(self, self->client_certs_file);
    break;
    default:
    break;
    }
}

static void
_eventd_evp_cleanup_monitors(EventdEvpContext *self)
{
    if ( self->cert_monitor != NULL )
    {
        g_file_monitor_cancel(self->cert_monitor);
        g_object_unref(self->cert_monitor);
    }
    g_free(self->cert_file);
    if ( self->key_monitor != NULL )
    {
        g_file_monitor_cancel(self->key_monitor);
        g_object_unref(self->key_monitor);
    }
    g_free(self->key_file);
    if ( self->client_certs_monitor != NULL )
    {
        g_file_monitor_cancel(self->client_certs_monitor);
        g_object_unref(self->client_certs_monitor);
    }
    g_free(self->client_certs_file);

    self->cert_monitor = NULL;
    self->cert_file = NULL;
    self->key_monitor = NULL;
    self->key_file = NULL;
    self->client_certs_monitor = NULL;
    self->client_certs_file = NULL;
}

static gboolean
_eventd_evp_monitor_files(EventdEvpContext *self, gchar *cert_file, gchar *key_file, gchar *client_certs_file)
{
    GError *error = NULL;
    GFile *file;
    GFileMonitor *cert_monitor;
    GFileMonitor *key_monitor = NULL;
    GFileMonitor *client_certs_monitor = NULL;

    file = g_file_new_for_path(cert_file);
    cert_monitor = g_file_monitor_file(file, G_FILE_MONITOR_NONE, NULL, &error);
    g_object_unref(file);
    if ( cert_monitor == NULL )
    {
        g_warning("Could not monitor certificate file %s: %s", cert_file, error->message);
        g_clear_error(&error);
        return FALSE;
    }

    if ( key_file != NULL )
    {
        file = g_file_new_for_path(key_file);
        key_monitor = g_file_monitor_file(file, G_FILE_MONITOR_NONE, NULL, &error);
        g_object_unref(file);
        if ( key_monitor == NULL )
        {
            g_warning("Could not monitor key file %s: %s", key_file, error->message);
            g_clear_error(&error);
            g_object_unref(cert_monitor);
            return FALSE;
        }
    }

    if ( client_certs_file != NULL )
    {
        file = g_file_new_for_path(client_certs_file);
        client_certs_monitor = g_file_monitor_file(file, G_FILE_MONITOR_NONE, NULL, &error);
        g_object_unref(file);
        if ( client_certs_monitor == NULL )
        {
            g_warning("Could not monitor client certificates file %s: %s", client_certs_file, error->message);
            g_clear_error(&error);
            if ( key_monitor != NULL )
                g_object_unref(key_monitor);
            g_object_unref(cert_monitor);
            return FALSE;
        }
    }

    _eventd_evp_cleanup_monitors(self);

    self->cert_monitor = cert_monitor;
    self->key_monitor = key_monitor;
    self->client_certs_monitor = client_certs_monitor;
    self->cert_file = cert_file;
    self->key_file = key_file;
    self->client_certs_file = client_certs_file;
    g_signal_connect_swapped(self->cert_monitor, "changed", G_CALLBACK(_eventd_evp_cert_key_file_changed), self);
    if ( self->key_monitor != NULL )
        g_signal_connect_swapped(self->key_monitor, "changed", G_CALLBACK(_eventd_evp_cert_key_file_changed), self);
    if ( self->client_certs_monitor != NULL )
        g_signal_connect_swapped(self->client_certs_monitor, "changed", G_CALLBACK(_eventd_evp_client_certs_file_changed), self);

    return TRUE;
}

void
eventd_evp_global_parse(EventdEvpContext *self, GKeyFile *config_file)
{
    gchar *cert_file = NULL;
    gchar *key_file = NULL;
    gchar *client_certs_file = NULL;
    gchar *ws_secret = NULL;
    gchar *publish_name = NULL;

    if ( ! g_key_file_has_group(config_file, "Server") )
        return;

    if ( evhelpers_config_key_file_get_string(config_file, "Server", "TLSCertificate", &cert_file) < 0 )
        goto cleanup;
    if ( evhelpers_config_key_file_get_string(config_file, "Server", "TLSKey", &key_file) < 0 )
        goto cleanup;
    if ( evhelpers_config_key_file_get_string(config_file, "Server", "TLSClientCertificates", &client_certs_file) < 0 )
        goto cleanup;
    if ( evhelpers_config_key_file_get_string(config_file, "Server", "WebSocketSecret", &ws_secret) < 0 )
        goto cleanup;
    if ( evhelpers_config_key_file_get_string(config_file, "Server", "PublishName", &publish_name) < 0 )
        goto cleanup;

    g_free(self->ws_secret);
    self->ws_secret = ws_secret;
    ws_secret = NULL;

    if ( cert_file != NULL )
    {
        if ( ! g_tls_backend_supports_tls(g_tls_backend_get_default()) )
            g_warning("TLS not suported");
        else if ( _eventd_evp_load_certificate(self, cert_file, key_file) )
        {
            if ( ( client_certs_file != NULL ) && ( ! _eventd_evp_load_client_certificates(self, client_certs_file) ) )
            {
                g_free(client_certs_file);
                client_certs_file = NULL;
            }
            if ( _eventd_evp_monitor_files(self, cert_file, key_file, client_certs_file) )
                cert_file = key_file = client_certs_file = NULL;
        }
    }
    else if ( key_file != NULL )
        g_warning("You need to configure a certificate file to add TLS support");

    if ( publish_name != NULL )
        eventd_sd_modules_set_publish_name(publish_name);

cleanup:
    g_free(publish_name);
    g_free(ws_secret);
    g_free(client_certs_file);
    g_free(key_file);
    g_free(cert_file);
}

void
eventd_evp_config_reset(EventdEvpContext *self)
{
    if ( self->certificate != NULL )
        g_object_unref(self->certificate);
    self->certificate = NULL;
    g_free(self->ws_secret);
    self->ws_secret = NULL;
    _eventd_evp_cleanup_monitors(self);
}


/*
 * Event dispatching interface
 */

void
eventd_evp_event_dispatch(EventdEvpContext *self, EventdEvent *event)
{
    const gchar *category;
    GList *subscribers;
    GList *client;

    category = eventd_event_get_category(event);
    if ( category[0] == '.' )
    {
        for ( client = self->clients ; client != NULL ; client = g_list_next(client) )
            eventd_evp_client_event_dispatch(client->data, event);
        return;
    }

    subscribers = self->subscribe_all;
    for ( client = subscribers ; client != NULL ; client = g_list_next(client) )
        eventd_evp_client_event_dispatch(client->data, event);

    subscribers = g_hash_table_lookup(self->subscribe_categories, category);
    for ( client = subscribers ; client != NULL ; client = g_list_next(client) )
        eventd_evp_client_event_dispatch(client->data, event);
}
