/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2012 Quentin "Sardem FF7" Glidic
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

#include "avahi.h"
#include "client.h"

#include "evp.h"

#ifdef ENABLE_AVAHI
#define AVAHI_OPTION_FLAG 0
#else /* ! ENABLE_AVAHI */
#define AVAHI_OPTION_FLAG G_OPTION_FLAG_HIDDEN
#endif /* ! ENABLE_AVAHI */

/*
 * Initialization interface
 */

static EventdPluginContext *
_eventd_evp_init(EventdPluginCoreContext *core, EventdPluginCoreInterface *core_interface)
{
    EventdPluginContext *self;

    self = g_new0(EventdPluginContext, 1);

    self->core = core;
    self->core_interface= core_interface;

    self->avahi_name = g_strdup_printf(PACKAGE_NAME " %s", g_get_host_name());

    return self;
}

static void
_eventd_evp_uninit(EventdPluginContext *self)
{
    g_free(self->avahi_name);

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
    sockets = eventd_plugin_core_get_sockets(self->core, self->core_interface, binds);

    GList *socket_;
    for ( socket_ = sockets ; socket_ != NULL ; socket_ = g_list_next(socket_) )
    {
        GError *error = NULL;

        if ( ! g_socket_listener_add_socket(G_SOCKET_LISTENER(self->service), socket_->data, NULL, &error) )
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
_eventd_evp_start(EventdPluginContext *self)
{
    GList *sockets = NULL;

    self->service = g_socket_service_new();

    if ( self->binds != NULL )
        sockets = _eventd_evp_add_socket(sockets, self, (const gchar * const *)self->binds);

#ifdef HAVE_GIO_UNIX
    if ( self->default_unix )
    {
        const gchar *binds[] = { "unix-runtime:" EVP_UNIX_SOCKET, NULL };
        sockets = _eventd_evp_add_socket(sockets, self, binds);
    }
#endif /* HAVE_GIO_UNIX */

    g_signal_connect(self->service, "incoming", G_CALLBACK(eventd_evp_client_connection_handler), self);

#ifdef ENABLE_AVAHI
    if ( ! self->no_avahi )
        self->avahi = eventd_evp_avahi_start(self->avahi_name, sockets);
    else
#endif /* ENABLE_AVAHI */
        g_list_free_full(sockets, g_object_unref);
}

static void
_eventd_evp_stop(EventdPluginContext *self)
{
#ifdef ENABLE_AVAHI
    eventd_evp_avahi_stop(self->avahi);
#endif /* ENABLE_AVAHI */

    g_list_free_full(self->clients, eventd_evp_client_disconnect);

    g_socket_service_stop(self->service);
    g_socket_listener_close(G_SOCKET_LISTENER(self->service));
    g_object_unref(self->service);
}


/*
 * Command-line options interface
 */

static GOptionGroup *
_eventd_evp_get_option_group(EventdPluginContext *self)
{
    GOptionGroup *option_group;
    GOptionEntry entries[] =
    {
        { "listen",         'l', 0,                 G_OPTION_ARG_STRING_ARRAY, &self->binds,        "Add a socket to listen to",     "<socket>" },
#ifdef HAVE_GIO_UNIX
        { "listen-default", 'u', 0,                 G_OPTION_ARG_NONE,         &self->default_unix, "Listen on default UNIX socket", NULL },
#endif /* HAVE_GIO_UNIX */
        { "no-avahi",       'A', AVAHI_OPTION_FLAG, G_OPTION_ARG_NONE,         &self->no_avahi,     "Disable avahi publishing",      NULL },
        { NULL }
    };

    option_group = g_option_group_new("event", "EVENT protocol plugin options", "Show EVENT plugin help options", NULL, NULL);
    g_option_group_set_translation_domain(option_group, GETTEXT_PACKAGE);
    g_option_group_add_entries(option_group, entries);
    return option_group;
}


/*
 * Configuration interface
 */

static void
_eventd_evp_global_parse(EventdPluginContext *self, GKeyFile *config_file)
{
    gchar *avahi_name;

    if ( ! g_key_file_has_group(config_file, "Server") )
        return;

    if ( evhelpers_config_key_file_get_string(config_file, "Server", "AvahiName", &avahi_name) < 0 )
        return;
    if ( avahi_name != NULL )
    {
        g_free(self->avahi_name);
        self->avahi_name = avahi_name;
    }
}

static void
_eventd_evp_config_reset(EventdPluginContext *self)
{
    g_free(self->avahi_name);
    self->avahi_name = NULL;
}


/*
 * Plugin interface
 */

EVENTD_EXPORT const gchar *eventd_plugin_id = "evp";
EVENTD_EXPORT
void
eventd_plugin_get_interface(EventdPluginInterface *interface)
{
    eventd_plugin_interface_add_init_callback(interface, _eventd_evp_init);
    eventd_plugin_interface_add_uninit_callback(interface, _eventd_evp_uninit);

    eventd_plugin_interface_add_get_option_group_callback(interface, _eventd_evp_get_option_group);

    eventd_plugin_interface_add_start_callback(interface, _eventd_evp_start);
    eventd_plugin_interface_add_stop_callback(interface, _eventd_evp_stop);

    eventd_plugin_interface_add_global_parse_callback(interface, _eventd_evp_global_parse);
    eventd_plugin_interface_add_config_reset_callback(interface, _eventd_evp_config_reset);
}
