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

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <libeventd-event.h>
#include <eventd-plugin.h>
#include <libeventd-config.h>

#include "types.h"

#include "avahi.h"

#include "server.h"

struct _EventdPluginContext {
    EventdRelayAvahi *avahi;
    GHashTable *servers;
    GHashTable *events;
};

static void
_eventd_relay_server_list_free(gpointer data)
{
    GList *list = data;
    g_list_free(list);
}

static EventdPluginContext *
_eventd_relay_init(EventdCoreContext *core, EventdCoreInterface *interface)
{
    EventdPluginContext *context;

    context = g_new0(EventdPluginContext, 1);

    context->events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, _eventd_relay_server_list_free);

    context->avahi = eventd_relay_avahi_init();
    context->servers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, eventd_relay_server_free);

    return context;
}

static void
_eventd_relay_uninit(EventdPluginContext *context)
{
    g_hash_table_unref(context->servers);
    eventd_relay_avahi_uninit(context->avahi);

    g_hash_table_unref(context->events);

    g_free(context);
}

static void
_eventd_relay_config_reset(EventdPluginContext *context)
{
    g_hash_table_remove_all(context->events);
}

static void
_eventd_relay_event_parse(EventdPluginContext *context, const gchar *id, GKeyFile *config_file)
{
    gboolean disable;
    gchar **server_uris = NULL;
    gchar **server_uri = NULL;
    gchar **avahi_names = NULL;
    gchar **avahi_name = NULL;
    GList *list = NULL;
    EventdRelayServer *server;

    if ( ! g_key_file_has_group(config_file, "Relay") )
        return;

    if ( libeventd_config_key_file_get_boolean(config_file, "Relay", "Disable", &disable) < 0 )
        return;
    if ( libeventd_config_key_file_get_string_list(config_file, "Relay", "Servers", &server_uris, NULL) < 0 )
        goto fail;
    if ( libeventd_config_key_file_get_string_list(config_file, "Relay", "Avahi", &avahi_names, NULL) < 0 )
        goto fail;

    if ( ! disable )
    {
        if ( server_uris != NULL )
        {
            for ( server_uri = server_uris ; *server_uri != NULL ; ++server_uri )
            {
                server = g_hash_table_lookup(context->servers, *server_uri);
                if ( server == NULL )
                {
                    server = eventd_relay_server_new(*server_uri);
                    if ( server == NULL )
                        g_warning("Couldn't create the connectiont to server '%s'", *server_uri);
                    else
                        g_hash_table_insert(context->servers, g_strdup(*server_uri), server);
                }
                if ( server != NULL )
                    list = g_list_prepend(list, server);
            }
        }

        if ( avahi_names != NULL )
        {
            for ( avahi_name = avahi_names ; *avahi_name != NULL ; ++avahi_name )
            {
                server = g_hash_table_lookup(context->servers, *avahi_name);
                if ( server == NULL )
                {
                    server = eventd_relay_server_new_avahi(context->avahi, *avahi_name);
                    if ( server != NULL )
                        g_hash_table_insert(context->servers, g_strdup(*avahi_name), server);
                }
                if ( server != NULL )
                    list = g_list_prepend(list, server);
            }
        }
    }

    g_hash_table_insert(context->events, g_strdup(id), list);

fail:
    if ( avahi_names != NULL )
        g_strfreev(avahi_names);
    if ( server_uris != NULL )
        g_strfreev(server_uris);
}

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
    g_hash_table_foreach(context->servers, _eventd_relay_stop_each, NULL);
}

static void
_eventd_relay_event_action(EventdPluginContext *context, EventdEvent *event)
{
    GList *servers;
    GList *server;

    servers = g_hash_table_lookup(context->events, eventd_event_get_config_id(event));
    if ( servers == NULL )
        return;

    for ( server = servers ; server != NULL ; server = g_list_next(server) )
        eventd_relay_server_event(server->data, event);
}

EVENTD_EXPORT const gchar *eventd_plugin_id = "eventd-relay";
EVENTD_EXPORT
void
eventd_plugin_get_info(EventdPlugin *plugin)
{
    plugin->init = _eventd_relay_init;
    plugin->uninit = _eventd_relay_uninit;

    plugin->start = _eventd_relay_start;
    plugin->stop = _eventd_relay_stop;

    plugin->config_reset = _eventd_relay_config_reset;

    plugin->event_parse = _eventd_relay_event_parse;
    plugin->event_action = _eventd_relay_event_action;
}

