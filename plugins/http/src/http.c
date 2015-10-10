/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2015 Quentin "Sardem FF7" Glidic
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
#include <libsoup/soup.h>

#include <libeventd-event.h>
#include <eventd-plugin.h>
#include <libeventd-helpers-config.h>

#include "websocket.h"

#include "http.h"

/*
 * Initialization interface
 */

static EventdPluginContext *
_eventd_http_init(EventdPluginCoreContext *core, EventdPluginCoreInterface *core_interface)
{
    EventdPluginContext *self;

    self = g_new0(EventdPluginContext, 1);

    self->core = core;
    self->core_interface= core_interface;

    return self;
}

static void
_eventd_http_uninit(EventdPluginContext *self)
{
    g_free(self);
}


/*
 * Start/Stop interface
 */

static GList *
_eventd_http_add_socket(GList *used_sockets, EventdPluginContext *self, const gchar * const *binds)
{
    GList *sockets;
    sockets = eventd_plugin_core_get_sockets(self->core, self->core_interface, binds);

    GList *socket_;
    for ( socket_ = sockets ; socket_ != NULL ; socket_ = g_list_next(socket_) )
    {
        GError *error = NULL;

        if ( ! soup_server_listen_socket(self->server, socket_->data, SOUP_SERVER_LISTEN_HTTPS, &error) )
        {
            g_warning("Unable to add socket: %s", error->message);
            g_clear_error(&error);
        }
        else
            used_sockets = g_list_prepend(used_sockets, g_object_ref(socket_->data));
    }
    g_list_free_full(sockets, g_object_unref);

    return used_sockets;
}

static void
_eventd_http_start(EventdPluginContext *self)
{
    GList *sockets = NULL;
    gchar *aliases[] = { "*", NULL };
    gchar *protocols[] = { EVP_SERVICE_NAME, EVP_SERVICE_NAME "-json", NULL };

    if ( self->certificate == NULL )
        return;

    self->server = soup_server_new(SOUP_SERVER_TLS_CERTIFICATE, self->certificate, SOUP_SERVER_HTTPS_ALIASES, aliases, NULL);

    if ( self->binds != NULL )
        sockets = _eventd_http_add_socket(sockets, self, (const gchar * const *)self->binds);

    //soup_server_add_handler(self->server, "/event", _eventd_http_handler_event, self, NULL);
    soup_server_add_websocket_handler(self->server, NULL, NULL, protocols, eventd_http_websocket_client_handler, self, NULL);

    g_list_free_full(sockets, g_object_unref);
}

static void
_eventd_http_stop(EventdPluginContext *self)
{
    g_list_free_full(self->clients, eventd_http_websocket_client_disconnect);
    self->clients = NULL;

    if ( self->server != NULL )
        g_object_unref(self->server);
}


/*
 * Command-line options interface
 */

static GOptionGroup *
_eventd_http_get_option_group(EventdPluginContext *self)
{
    GOptionGroup *option_group;
    GOptionEntry entries[] =
    {
        { "listen-http", 0, 0, G_OPTION_ARG_STRING_ARRAY, &self->binds, "Add a socket to listen to", "<socket>" },
        { NULL }
    };

    option_group = g_option_group_new("http", "HTTP plugin options", "Show HTTP plugin help options", NULL, NULL);
    g_option_group_set_translation_domain(option_group, GETTEXT_PACKAGE);
    g_option_group_add_entries(option_group, entries);
    return option_group;
}


/*
 * Configuration interface
 */

static void
_eventd_http_global_parse(EventdPluginContext *self, GKeyFile *config_file)
{
    gchar *tls_certificate = NULL;
    gchar *tls_key = NULL;

    if ( ! g_key_file_has_group(config_file, "Server") )
        return;

    if ( evhelpers_config_key_file_get_string(config_file, "Server", "TLSCertificate", &tls_certificate) != 0 )
        goto error;
    if ( evhelpers_config_key_file_get_string(config_file, "Server", "TLSKey", &tls_key) < 0 )
        goto error;

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
}

static void
_eventd_http_config_reset(EventdPluginContext *self)
{
    if ( self->certificate != NULL )
        g_object_unref(self->certificate);
    self->certificate = NULL;
}


/*
 * Plugin interface
 */

EVENTD_EXPORT const gchar *eventd_plugin_id = "http";
EVENTD_EXPORT
void
eventd_plugin_get_interface(EventdPluginInterface *interface)
{
    eventd_plugin_interface_add_init_callback(interface, _eventd_http_init);
    eventd_plugin_interface_add_uninit_callback(interface, _eventd_http_uninit);

    eventd_plugin_interface_add_get_option_group_callback(interface, _eventd_http_get_option_group);

    eventd_plugin_interface_add_start_callback(interface, _eventd_http_start);
    eventd_plugin_interface_add_stop_callback(interface, _eventd_http_stop);

    eventd_plugin_interface_add_global_parse_callback(interface, _eventd_http_global_parse);
    eventd_plugin_interface_add_config_reset_callback(interface, _eventd_http_config_reset);
}
