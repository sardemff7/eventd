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
#include "dns-sd.h"
#include "ssdp.h"


struct _EventdPluginContext {
    EventdPluginCoreContext *core;
    EventdRelayDNSSD *dns_sd;
    EventdRelaySSDP *ssdp;
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

    context->dns_sd = eventd_relay_dns_sd_init();
    context->ssdp = eventd_relay_ssdp_init();
    context->servers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, eventd_relay_server_free);

    return context;
}

static void
_eventd_relay_uninit(EventdPluginContext *context)
{
    g_hash_table_unref(context->servers);
    eventd_relay_ssdp_uninit(context->ssdp);
    eventd_relay_dns_sd_uninit(context->dns_sd);

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
    eventd_relay_dns_sd_start(context->dns_sd);
    eventd_relay_ssdp_start(context->ssdp);
}

static void
_eventd_relay_stop_each(gpointer key, gpointer data, gpointer user_data)
{
    if ( data == NULL )
        return;

    if ( eventd_relay_server_has_address(data) )
        eventd_relay_server_stop(data);
}

static void
_eventd_relay_stop(EventdPluginContext *context)
{
    eventd_relay_ssdp_stop(context->ssdp);
    eventd_relay_dns_sd_stop(context->dns_sd);
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
    else if ( g_strcmp0(argv[0], "status") == 0 )
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
            gboolean has_address, connected;
            has_address = eventd_relay_server_has_address(server);
            connected = eventd_relay_server_is_connected(server);

            const gchar *s = "is connected";
            if ( ! has_address )
            {
                r = EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_1;
                s = "has no address";
            }
            else if ( ! connected )
            {
                r = EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_2;
                s = "is disconnected";
            }
            *status = g_strdup_printf("Server '%s' %s", argv[1], s);
        }
        GHashTableIter iter;
        g_hash_table_iter_init(&iter, context->servers);
    }
    else if ( g_strcmp0(argv[0], "list") == 0 )
    {
        if ( g_hash_table_size(context->servers) == 0 )
        {
            r = EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_1;
            *status = g_strdup("No server");
        }
        else
        {
            GString *list;
            list = g_string_sized_new(strlen("Server list:") + strlen("\n    a quit long name") * g_hash_table_size(context->servers));
            g_string_append(list, "Server list:");
            GHashTableIter iter;
            const gchar *name;
            g_hash_table_iter_init(&iter, context->servers);
            while ( g_hash_table_iter_next(&iter, (gpointer *) &name, NULL) )
                g_string_append(g_string_append(list, "\n    "), name);

            *status = g_string_free(list, FALSE);
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
_eventd_relay_server_parse(EventdPluginContext *context, GKeyFile *config_file, gchar *server_name)
{
    g_hash_table_remove(context->servers, server_name);

    gsize size = strlen("Relay ") + strlen(server_name) + 1;
    gchar group[size];
    g_snprintf(group, size, "Relay %s", server_name);
    if ( ! g_key_file_has_group(config_file, group) )
        return;

    gchar *server_uri = NULL;
    gchar *discover_name = NULL;

    /* Ensure we have at least one way to connect to the server */
    if ( ( context->dns_sd != NULL ) || ( context->ssdp != NULL ) )
    {
        if ( evhelpers_config_key_file_get_string(config_file, group, "DiscoverName", &discover_name) < 0 )
            return;
    }
    if ( discover_name == NULL )
    {
        if ( evhelpers_config_key_file_get_string(config_file, group, "Server", &server_uri) != 0 )
            return;
    }

    gboolean accept_unknown_ca = FALSE;
    gchar *server_identity = NULL;
    gchar **forwards = NULL;
    gchar **subscriptions = NULL;

    if ( evhelpers_config_key_file_get_string(config_file, group, "ServerIdentity", &server_identity) < 0 )
        goto cleanup;
    if ( evhelpers_config_key_file_get_boolean(config_file, group, "AcceptUnknownCA", &accept_unknown_ca) < 0 )
        goto cleanup;
    if ( evhelpers_config_key_file_get_string_list(config_file, group, "Forwards", &forwards, NULL) < 0 )
        goto cleanup;
    if ( evhelpers_config_key_file_get_string_list(config_file, group, "Subscriptions", &subscriptions, NULL) < 0 )
        goto cleanup;

    EventdRelayServer *server;
    if ( discover_name != NULL )
    {
        server = eventd_relay_server_new(context->core, server_identity, accept_unknown_ca, forwards, subscriptions);
        eventd_relay_dns_sd_monitor_server(context->dns_sd, discover_name, server);
        eventd_relay_ssdp_monitor_server(context->ssdp, discover_name, server);
    }
    else
    {
        server = eventd_relay_server_new_for_domain(context->core, server_identity, accept_unknown_ca, forwards, subscriptions, server_uri);
        if ( server == NULL )
        {
            g_warning("Couldn't create the connection to server '%s' using '%s'", server_name, server_uri);
            goto cleanup;
        }
    }

    g_hash_table_insert(context->servers, server_name, server);
    server_name = NULL;
    forwards = subscriptions = NULL;

cleanup:
    g_strfreev(subscriptions);
    g_strfreev(forwards);
    g_free(server_identity);
    g_free(discover_name);
    g_free(server_uri);
    g_free(server_name);
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
    g_free(servers);
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

