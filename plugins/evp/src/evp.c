/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2016 Quentin "Sardem FF7" Glidic
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

#include <config.h>

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <libeventd-event.h>
#include <eventd-plugin.h>
#include <libeventd-helpers-config.h>

#include "dns-sd.h"
#include "client.h"

#include "evp.h"

#ifdef G_OS_UNIX
#define DEFAULT_SOCKET_BIND_PREFIX "unix"
#else /* ! G_OS_UNIX */
#define DEFAULT_SOCKET_BIND_PREFIX "tcp-file"
#endif /* ! G_OS_UNIX */

/*
 * Initialization interface
 */

static EventdPluginContext *
_eventd_evp_init(EventdPluginCoreContext *core)
{
    EventdPluginContext *self;

    self = g_new0(EventdPluginContext, 1);

    self->core = core;

    return self;
}

static void
_eventd_evp_uninit(EventdPluginContext *self)
{
    g_free(self->publish_name);

    if ( self->binds != NULL )
        g_strfreev(self->binds);

    g_free(self);
}


/*
 * Start/Stop interface
 */

static GList *
_eventd_evp_add_socket(GList *used_sockets, EventdPluginContext *self, const gchar * const *binds)
{
    GList *sockets;
    sockets = eventd_plugin_core_get_sockets(self->core, binds);

    GList *socket_;
    for ( socket_ = sockets ; socket_ != NULL ; socket_ = g_list_next(socket_) )
    {
        GSocket *socket = socket_->data;
        GError *error = NULL;

        if ( ! g_socket_listener_add_socket(G_SOCKET_LISTENER(self->service), socket, NULL, &error) )
        {
            g_warning("Unable to add socket: %s", error->message);
            g_clear_error(&error);
        }
        else
            used_sockets = g_list_prepend(used_sockets, g_object_ref(socket));
    }
    g_list_free_full(sockets, g_object_unref);

    return used_sockets;
}

static void
_eventd_evp_start(EventdPluginContext *self)
{
    GList *sockets = NULL;

    self->service = g_socket_service_new();

    if ( self->binds != NULL )
        sockets = _eventd_evp_add_socket(sockets, self, (const gchar * const *)self->binds);
    else
    {
        const gchar *binds[] = { DEFAULT_SOCKET_BIND_PREFIX "-runtime:" EVP_UNIX_SOCKET, "all", NULL };
        sockets = _eventd_evp_add_socket(sockets, self, binds);
    }

    g_signal_connect(self->service, "incoming", G_CALLBACK(eventd_evp_client_connection_handler), self);

    if ( self->publish_name != NULL )
    {
        self->dns_sd = eventd_evp_dns_sd_start(self->publish_name, sockets);
        self->ssdp = eventd_evp_ssdp_start(self->publish_name, sockets);
    }
    g_list_free_full(sockets, g_object_unref);

    self->subscribe_categories = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_list_free);
}

static void
_eventd_evp_stop(EventdPluginContext *self)
{
    eventd_evp_dns_sd_stop(self->dns_sd);

    g_hash_table_unref(self->subscribe_categories);
    self->subscribe_categories = NULL;
    g_list_free(self->subscribe_all);
    self->subscribe_all = NULL;

    g_list_free_full(self->clients, eventd_evp_client_disconnect);
    self->clients = NULL;

    g_socket_service_stop(self->service);
    g_socket_listener_close(G_SOCKET_LISTENER(self->service));
    g_object_unref(self->service);
}


/*
 * Configuration interface
 */

static void
_eventd_evp_global_parse(EventdPluginContext *self, GKeyFile *config_file)
{
    gchar **binds = NULL;
    gchar *cert_file = NULL;
    gchar *key_file = NULL;
    gchar *publish_name;

    if ( ! g_key_file_has_group(config_file, "Server") )
        return;

    if ( evhelpers_config_key_file_get_string_list(config_file, "Server", "Listen", &binds, NULL) < 0 )
        goto cleanup;
    if ( evhelpers_config_key_file_get_string(config_file, "Server", "TLSCertificate", &cert_file) < 0 )
        goto cleanup;
    if ( evhelpers_config_key_file_get_string(config_file, "Server", "TLSKey", &key_file) < 0 )
        goto cleanup;
    if ( evhelpers_config_key_file_get_string(config_file, "Server", "PublishName", &publish_name) < 0 )
        goto cleanup;

    GError *error = NULL;

    if ( binds != NULL )
    {
        gchar **tmp = self->binds;
        self->binds = binds;
        binds = tmp;
    }

    if ( cert_file != NULL )
    {
        if ( ! g_tls_backend_supports_tls(g_tls_backend_get_default()) )
            g_warning("TLS not suported");
        else
        {
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
            }
            else
            {
                if ( key_file != NULL )
                    g_warning("Could not read certificate (%s) or key (%s) file: %s", cert_file, key_file, error->message);
                else
                    g_warning("Could not read certificate file (%s): %s", cert_file, error->message);
                g_clear_error(&error);
            }
        }
    }
    else if ( key_file != NULL )
        g_warning("You need to configure a certificate file to add TLS support");

    if ( publish_name != NULL )
    {
        g_free(self->publish_name);
        self->publish_name = publish_name;
    }

cleanup:
    g_free(key_file);
    g_free(cert_file);
    g_strfreev(binds);
}

static void
_eventd_evp_config_reset(EventdPluginContext *self)
{
    if ( self->certificate != NULL )
        g_object_unref(self->certificate);
    self->certificate = NULL;

    g_free(self->publish_name);
    self->publish_name = NULL;
}


/*
 * Event dispatching interface
 */

static void
_eventd_evp_event_dispatch(EventdPluginContext *self, EventdEvent *event)
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


/*
 * Plugin interface
 */

EVENTD_EXPORT const gchar *eventd_plugin_id = "evp";
EVENTD_EXPORT const gboolean eventd_plugin_system_mode_support = TRUE;
EVENTD_EXPORT
void
eventd_plugin_get_interface(EventdPluginInterface *interface)
{
    eventd_plugin_interface_add_init_callback(interface, _eventd_evp_init);
    eventd_plugin_interface_add_uninit_callback(interface, _eventd_evp_uninit);

    eventd_plugin_interface_add_start_callback(interface, _eventd_evp_start);
    eventd_plugin_interface_add_stop_callback(interface, _eventd_evp_stop);

    eventd_plugin_interface_add_global_parse_callback(interface, _eventd_evp_global_parse);
    eventd_plugin_interface_add_config_reset_callback(interface, _eventd_evp_config_reset);

    eventd_plugin_interface_add_event_dispatch_callback(interface, _eventd_evp_event_dispatch);
}
