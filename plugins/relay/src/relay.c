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

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <glib.h>
#include <glib/gprintf.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <libeventd-event.h>
#include <eventd-plugin.h>
#include <libeventd-helpers-config.h>


#include "server.h"
#include "avahi.h"


struct _EventdPluginContext {
    EventdPluginCoreContext *core;
    EventdRelayAvahi *avahi;
    GHashTable *servers;
};


/*
 * Initialization interface
 */

static EventdPluginContext *
_eventd_relay_init(EventdPluginCoreContext *core)
{
    EventdPluginContext *context;

    context = g_new0(EventdPluginContext, 1);
    context->core = core;

#ifdef ENABLE_AVAHI
    context->avahi = eventd_relay_avahi_init();
#endif /* ENABLE_AVAHI */
    context->servers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, eventd_relay_server_free);

    return context;
}

static void
_eventd_relay_uninit(EventdPluginContext *context)
{
    g_hash_table_unref(context->servers);
#ifdef ENABLE_AVAHI
    eventd_relay_avahi_uninit(context->avahi);
#endif /* ENABLE_AVAHI */

    g_free(context);
}



/*
 * Start/Stop interface
 */

static void
_eventd_relay_start_each(gpointer key, gpointer data, gpointer user_data)
{
    if ( data == NULL )
        return;

    eventd_relay_server_start(data);
}

static void
_eventd_relay_start(EventdPluginContext *context)
{
    g_hash_table_foreach(context->servers, _eventd_relay_start_each, NULL);
#ifdef ENABLE_AVAHI
    eventd_relay_avahi_start(context->avahi);
#endif /* ENABLE_AVAHI */
}

static void
_eventd_relay_stop_each(gpointer key, gpointer data, gpointer user_data)
{
    if ( data == NULL )
        return;

    eventd_relay_server_stop(data);
}

static void
_eventd_relay_stop(EventdPluginContext *context)
{
#ifdef ENABLE_AVAHI
    eventd_relay_avahi_stop(context->avahi);
#endif /* ENABLE_AVAHI */
    g_hash_table_foreach(context->servers, _eventd_relay_stop_each, NULL);
}


/*
 * Control command interface
 */

static EventdPluginCommandStatus
_eventd_relay_control_command(EventdPluginContext *context, guint64 argc, const gchar * const *argv, gchar **status)
{
    EventdRelayServer *server;
    EventdPluginCommandStatus r = EVENTD_PLUGIN_COMMAND_STATUS_OK;

    if ( g_strcmp0(argv[0], "connect") == 0 )
    {
        if ( argc < 2 )
        {
            *status = g_strdup("No server specified");
            r = EVENTD_PLUGIN_COMMAND_STATUS_COMMAND_ERROR;
        }
        else if ( ( server = g_hash_table_lookup(context->servers, argv[1]) ) == NULL )
        {
            *status = g_strdup_printf("No such server '%s'", argv[1]);
            r = EVENTD_PLUGIN_COMMAND_STATUS_EXEC_ERROR;
        }
        else
        {
            eventd_relay_server_start(server);
            *status = g_strdup_printf("Connected to server '%s'", argv[1]);
        }
    }
    else if ( g_strcmp0(argv[0], "disconnect") == 0 )
    {
        if ( argc < 2 )
        {
            *status = g_strdup("No server specified");
            r = EVENTD_PLUGIN_COMMAND_STATUS_COMMAND_ERROR;
        }
        else if ( ( server = g_hash_table_lookup(context->servers, argv[1]) ) == NULL )
        {
            *status = g_strdup_printf("No such server '%s'", argv[1]);
            r = EVENTD_PLUGIN_COMMAND_STATUS_EXEC_ERROR;
        }
        else
        {
            eventd_relay_server_stop(server);
            *status = g_strdup_printf("Disconnected from server '%s'", argv[1]);
        }
    }
    else
    {
        *status = g_strdup_printf("Unknown command '%s'", argv[0]);
        r = EVENTD_PLUGIN_COMMAND_STATUS_COMMAND_ERROR;
    }

    return r;
}


/*
 * Configuration interface
 */

static void
_eventd_relay_server_parse(EventdPluginContext *context, GKeyFile *config_file, const gchar *server)
{
    gsize size = strlen("Relay ") + strlen(server) + 1;
    gchar group[size];
    g_snprintf(group, size, "Relay %s", server);
    if ( ! g_key_file_has_group(config_file, group) )
        return;

    gboolean accept_unknown_ca = FALSE;
    gchar *server_identity_ = NULL;
    GSocketConnectable *server_identity = NULL;
    gchar **forwards = NULL;
    gchar **subscriptions = NULL;
    gchar *server_uri = NULL;

    if ( evhelpers_config_key_file_get_string(config_file, group, "ServerIdentity", &server_identity_) < 0 )
        goto cleanup;
    else if ( server_identity_ != NULL )
    {
        server_identity = g_network_address_new(server_identity_, 0);
        g_free(server_identity_);
    }
    if ( evhelpers_config_key_file_get_boolean(config_file, group, "AcceptUnknownCA", &accept_unknown_ca) < 0 )
        goto cleanup;
    if ( evhelpers_config_key_file_get_string_list(config_file, group, "Forwards", &forwards, NULL) < 0 )
        goto cleanup;
    if ( evhelpers_config_key_file_get_string_list(config_file, group, "Subscriptions", &subscriptions, NULL) < 0 )
        goto cleanup;

#ifdef ENABLE_AVAHI
    gchar *avahi_name;
    if ( ( context->avahi != NULL ) && ( evhelpers_config_key_file_get_string(config_file, group, "Avahi", &avahi_name) == 0 ) )
    {
        EventdRelayServer *server;
        server = g_hash_table_lookup(context->servers, avahi_name);
        if ( server == NULL )
        {
            server = eventd_relay_server_new(context->core, server_identity, accept_unknown_ca, forwards, subscriptions);
            eventd_relay_avahi_monitor_server(context->avahi, avahi_name, server);
            g_hash_table_insert(context->servers, avahi_name, server);
            forwards = subscriptions = NULL;
        }
        else
            g_free(avahi_name);
    }
    else
#endif /* ENABLE_AVAHI */
    if ( evhelpers_config_key_file_get_string(config_file, group, "Server", &server_uri) == 0 )
    {
        EventdRelayServer *server;
        server = g_hash_table_lookup(context->servers, server_uri);
        if ( server == NULL )
        {
            server = eventd_relay_server_new_for_domain(context->core, server_identity, accept_unknown_ca, forwards, subscriptions, server_uri);
            if ( server == NULL )
                g_warning("Couldn't create the connection to server '%s'", server_uri);
            else
            {
                g_hash_table_insert(context->servers, server_uri, server);
                forwards = subscriptions = NULL;
            }
        }
        else
            g_free(server_uri);
    }

cleanup:
    g_strfreev(subscriptions);
    g_strfreev(forwards);
    if ( server_identity != NULL )
        g_object_unref(server_identity);
}

static void
_eventd_relay_global_parse(EventdPluginContext *context, GKeyFile *config_file)
{
    if ( ! g_key_file_has_group(config_file, "Relay") )
        return;

    gchar **servers = NULL;
    if ( evhelpers_config_key_file_get_string_list(config_file, "Relay", "Servers", &servers, NULL) < 0 )
        return;

    if ( servers == NULL )
        return;

    gchar **server;
    for ( server = servers ; *server != NULL ; ++server )
        _eventd_relay_server_parse(context, config_file, *server);
    g_strfreev(servers);
}

static void
_eventd_relay_config_reset(EventdPluginContext *context)
{
    g_hash_table_remove_all(context->servers);
}


/*
 * Event action interface
 */

static void
_eventd_relay_event_dispatch(EventdPluginContext *context, EventdEvent *event)
{
    GHashTableIter iter;
    gchar *name;
    EventdRelayServer *server;
    g_hash_table_iter_init(&iter, context->servers);
    while ( g_hash_table_iter_next(&iter, (gpointer *) &name, (gpointer *) &server) )
        eventd_relay_server_event(server, event);
}


/*
 * Plugin interface
 */

EVENTD_EXPORT const gchar *eventd_plugin_id = "relay";
EVENTD_EXPORT const gboolean eventd_plugin_system_mode_support = TRUE;
EVENTD_EXPORT
void
eventd_plugin_get_interface(EventdPluginInterface *interface)
{
    eventd_plugin_interface_add_init_callback(interface, _eventd_relay_init);
    eventd_plugin_interface_add_uninit_callback(interface, _eventd_relay_uninit);

    eventd_plugin_interface_add_start_callback(interface, _eventd_relay_start);
    eventd_plugin_interface_add_stop_callback(interface, _eventd_relay_stop);

    eventd_plugin_interface_add_control_command_callback(interface, _eventd_relay_control_command);

    eventd_plugin_interface_add_global_parse_callback(interface, _eventd_relay_global_parse);
    eventd_plugin_interface_add_config_reset_callback(interface, _eventd_relay_config_reset);

    eventd_plugin_interface_add_event_dispatch_callback(interface, _eventd_relay_event_dispatch);
}

