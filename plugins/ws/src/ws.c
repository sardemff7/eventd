/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2017 Quentin "Sardem FF7" Glidic
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
#include <libsoup/soup.h>
#include <gio/gio.h>

#include "libeventd-event.h"
#include "eventd-plugin.h"
#include "libeventd-helpers-config.h"

#include "ws-client.h"

#include "ws.h"

/*
 * Initialization interface
 */

static EventdPluginContext *
_evend_ws_init(EventdPluginCoreContext *core)
{
    EventdPluginContext *self;

    self = g_new0(EventdPluginContext, 1);

    self->core = core;

    return self;
}

static void
_evend_ws_uninit(EventdPluginContext *self)
{
    g_strfreev(self->binds);
    g_free(self);
}


/*
 * Start/Stop interface
 */

static const gchar * const _eventd_ws_default_binds[] = {
    "tcp-file-runtime:evp-ws",
    NULL
};

static void
_evend_ws_add_socket(EventdPluginContext *self, const gchar * const *binds)
{
    if ( binds == NULL )
        binds =_eventd_ws_default_binds;

    GList *sockets;
    sockets = eventd_plugin_core_get_binds(self->core, "evp-ws", binds);

    GList *socket_;
    for ( socket_ = sockets ; socket_ != NULL ; socket_ = g_list_next(socket_) )
    {
        GError *error = NULL;
        GSocketAddress *address;
        gboolean loopback = FALSE;

        address = g_socket_get_local_address(socket_->data, &error);
        if ( address == NULL )
        {
            g_warning("Could not get socket local address: %s", error->message);
            continue;
        }
        switch ( g_socket_address_get_family(address) )
        {
        case G_SOCKET_FAMILY_IPV4:
        case G_SOCKET_FAMILY_IPV6:
            loopback = g_inet_address_get_is_loopback(g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(address)));
        break;
        case G_SOCKET_FAMILY_UNIX:
            loopback = TRUE;
        break;
        case G_SOCKET_FAMILY_INVALID:
            g_return_if_reached();
        }

        if ( loopback )
        {
            if ( ! soup_server_listen_socket(self->server, socket_->data, 0, &error) )
            {
                g_warning("Unable to add socket: %s", error->message);
                g_clear_error(&error);
            }
        }
        else
        {
            if ( self->certificate == NULL )
                g_warning("A non-loopback socket was requested but no certificate to use");
            else if ( ! soup_server_listen_socket(self->server, socket_->data, SOUP_SERVER_LISTEN_HTTPS, &error) )
            {
                g_warning("Unable to add socket: %s", error->message);
                g_clear_error(&error);
            }
        }
    }
    g_list_free_full(sockets, g_object_unref);
}

static gboolean
_eventd_ws_auth_domain_basic_callback(SoupAuthDomain* domain, SoupServerMessage* msg, const char* username, const char* password, gpointer user_data)
{
    EventdPluginContext *self = user_data;
    if ( ( ( username == NULL ) || ( *username == '\0' ) ) && ( g_strcmp0(password, self->secret) == 0 ) )
        return TRUE;
    return FALSE;
}

static void
_evend_ws_start(EventdPluginContext *self)
{
    gchar *protocols[] = { EVP_SERVICE_NAME, NULL };

    self->server = soup_server_new(NULL, NULL);
    if ( self->certificate != NULL )
        soup_server_set_tls_certificate(self->server, self->certificate);
    if ( self->secret != NULL )
    {
        SoupAuthDomain *auth_domain = soup_auth_domain_basic_new("realm", "eventd", NULL);
        soup_auth_domain_add_path(auth_domain, "/");
        soup_auth_domain_basic_set_auth_callback(auth_domain, _eventd_ws_auth_domain_basic_callback, self, NULL);
        soup_server_add_auth_domain(self->server, auth_domain);
        g_object_unref(auth_domain);
    }

    _evend_ws_add_socket(self, (const gchar * const *)self->binds);

    soup_server_add_websocket_handler(self->server, NULL, NULL, protocols, evend_ws_websocket_client_handler, self, NULL);

    self->subscribe_categories = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_list_free);
}

static void
_evend_ws_stop(EventdPluginContext *self)
{
    if ( self->server == NULL )
        return;

    g_hash_table_unref(self->subscribe_categories);
    self->subscribe_categories = NULL;
    g_list_free(self->subscribe_all);
    self->subscribe_all = NULL;

    g_list_free_full(self->clients, evend_ws_websocket_client_disconnect);
    self->clients = NULL;

    g_object_unref(self->server);
}


/*
 * Configuration interface
 */

static void
_evend_ws_global_parse(EventdPluginContext *self, GKeyFile *config_file)
{
    gchar **binds = NULL;
    gchar *secret = NULL;
    gchar *tls_certificate = NULL;
    gchar *tls_key = NULL;

    if ( ! g_key_file_has_group(config_file, "Server") )
        return;

    if ( evhelpers_config_key_file_get_string_list(config_file, "Server", "ListenWS", &binds, NULL) < 0 )
        goto error;
    if ( evhelpers_config_key_file_get_string(config_file, "Server", "WebSocketSecret", &secret) < 0 )
        goto error;
    if ( evhelpers_config_key_file_get_string(config_file, "Server", "TLSCertificate", &tls_certificate) < 0 )
        goto error;
    if ( evhelpers_config_key_file_get_string(config_file, "Server", "TLSKey", &tls_key) < 0 )
        goto error;

    g_free(self->secret);
    self->secret = secret;
    secret = NULL;

    if ( binds != NULL )
    {
        self->binds = binds;
        binds = NULL;
    }

    if ( tls_certificate != NULL )
    {
        GTlsCertificate *certificate;
        GError *error = NULL;

        if ( tls_key != NULL )
            certificate = g_tls_certificate_new_from_files(tls_certificate, tls_key, &error);
        else
            certificate = g_tls_certificate_new_from_file(tls_certificate, &error);

        if ( certificate == NULL )
            g_warning("Couldn't read certificate file %s: %s", tls_certificate, error->message);
        else
        {
            if ( self->certificate != NULL )
                g_object_unref(self->certificate);
            self->certificate = certificate;
        }
        g_clear_error(&error);
    }

error:
    g_free(tls_key);
    g_free(tls_certificate);
    g_free(secret);
    g_strfreev(binds);
}

static void
_evend_ws_config_reset(EventdPluginContext *self)
{
    if ( self->certificate != NULL )
        g_object_unref(self->certificate);
    self->certificate = NULL;

    g_free(self->secret);
    self->secret = NULL;

    g_strfreev(self->binds);
    self->binds = NULL;
}


/*
 * Event dispatching interface
 */

static void
_evend_ws_event_dispatch(EventdPluginContext *self, EventdEvent *event)
{
    if ( self->server == NULL )
        return;

    const gchar *category;
    GList *subscribers;
    GList *client;

    category = eventd_event_get_category(event);
    if ( category[0] == '.' )
    {
        for ( client = self->clients ; client != NULL ; client = g_list_next(client) )
            evend_ws_websocket_client_event_dispatch(client->data, event);
        return;
    }

    subscribers = self->subscribe_all;
    for ( client = subscribers ; client != NULL ; client = g_list_next(client) )
        evend_ws_websocket_client_event_dispatch(client->data, event);

    subscribers = g_hash_table_lookup(self->subscribe_categories, eventd_event_get_category(event));
    for ( client = subscribers ; client != NULL ; client = g_list_next(client) )
        evend_ws_websocket_client_event_dispatch(client->data, event);
}


/*
 * Plugin interface
 */

EVENTD_EXPORT const gchar *eventd_plugin_id = "ws";
EVENTD_EXPORT const gboolean eventd_plugin_system_mode_support = TRUE;
EVENTD_EXPORT
void
eventd_plugin_get_interface(EventdPluginInterface *interface)
{
    eventd_plugin_interface_add_init_callback(interface, _evend_ws_init);
    eventd_plugin_interface_add_uninit_callback(interface, _evend_ws_uninit);

    eventd_plugin_interface_add_start_callback(interface, _evend_ws_start);
    eventd_plugin_interface_add_stop_callback(interface, _evend_ws_stop);

    eventd_plugin_interface_add_global_parse_callback(interface, _evend_ws_global_parse);
    eventd_plugin_interface_add_config_reset_callback(interface, _evend_ws_config_reset);

    eventd_plugin_interface_add_event_dispatch_callback(interface, _evend_ws_event_dispatch);
}
