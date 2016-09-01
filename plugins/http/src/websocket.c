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

#include "config.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <glib.h>
#include <glib-object.h>
#include <libsoup/soup.h>

#include "libeventd-event.h"
#include "libeventd-protocol.h"
#include "eventd-plugin.h"
#include "libeventd-helpers-config.h"

#include "http.h"
#include "websocket.h"

struct _EventdHttpWebsocketClient {
    EventdPluginContext *context;
    GList *link;
    EventdProtocol *protocol;
    SoupWebsocketConnection *connection;
    GList *subscribe_all;
    GHashTable *subscriptions;
    EventdEvent *current;
};

static void
_eventd_http_websocket_client_send_message(EventdHttpWebsocketClient *self, gchar *message)
{
    gsize s;
    gchar *c = message, *oc;
    s = strlen(message);
    while ( ( c = g_utf8_strchr(oc = c, s, '\n') ) != NULL )
    {
        s -= ( c - oc );
        *c++ = '\0';
        soup_websocket_connection_send_text(self->connection, oc);
    }
    g_free(message);
}

static void
_eventd_http_websocket_client_protocol_event(EventdProtocol *protocol, EventdEvent *event, gpointer user_data)
{
    EventdHttpWebsocketClient *self = user_data;

#ifdef EVENTD_DEBUG
    g_debug("Received an event (category: %s): %s", eventd_event_get_category(event), eventd_event_get_name(event));
#endif /* EVENTD_DEBUG */

    self->current = event;
    eventd_plugin_core_push_event(self->context->core, event);
    self->current = NULL;
}

static void
_eventd_http_websocket_client_protocol_subscribe(EventdProtocol *protocol, GHashTable *categories, gpointer user_data)
{
    EventdHttpWebsocketClient *self = user_data;

    if ( categories == NULL )
    {
        self->context->subscribe_all = g_list_prepend(self->context->subscribe_all, self);
        self->subscribe_all = self->context->subscribe_all;
        return;
    }

    GHashTableIter iter;
    gchar *category;
    gpointer dummy;
    gchar *key = NULL;
    GList *list = NULL;
    g_hash_table_iter_init(&iter, categories);
    while ( g_hash_table_iter_next(&iter, (gpointer *) &category, &dummy) )
    {
        if ( g_hash_table_lookup_extended(self->context->subscribe_categories, category, (gpointer *) &key, (gpointer *) &list) )
            g_hash_table_steal(self->context->subscribe_categories, category);
        else
            key = g_strdup(category);

        list = g_list_prepend(list, self);

        g_hash_table_insert(self->context->subscribe_categories, key, list);
        g_hash_table_insert(self->subscriptions, key, list);
    }
}

static void
_eventd_http_websocket_client_protocol_bye(EventdProtocol *protocol, const gchar *message, gpointer user_data)
{
    EventdHttpWebsocketClient *self = user_data;

#ifdef EVENTD_DEBUG
    g_debug("Client connection closed");
#endif /* EVENTD_DEBUG */

    soup_websocket_connection_close(self->connection, SOUP_WEBSOCKET_CLOSE_PROTOCOL_ERROR, "BYE not supported for WebSockets");
}

static const EventdProtocolCallbacks _eventd_http_websocket_client_protocol_callbacks = {
    .event = _eventd_http_websocket_client_protocol_event,
    .subscribe = _eventd_http_websocket_client_protocol_subscribe,
    .bye = _eventd_http_websocket_client_protocol_bye
};

static void
_eventd_http_websocket_client_message(EventdHttpWebsocketClient *self, gint type, GBytes *message, SoupWebsocketConnection *connection)
{
    GError *error = NULL;

    if ( type != SOUP_WEBSOCKET_DATA_TEXT )
        soup_websocket_connection_close(self->connection, SOUP_WEBSOCKET_CLOSE_UNSUPPORTED_DATA, "Data must be UTF-8 text");
    else
    {
        gconstpointer data;
        data = g_bytes_get_data(message, NULL);
        if ( ! eventd_protocol_parse(self->protocol, data, &error) )
        {
            g_warning("Parse error: %s", error->message);
            soup_websocket_connection_close(self->connection, SOUP_WEBSOCKET_CLOSE_PROTOCOL_ERROR, error->message);
        }
    }
    g_clear_error(&error);
}

static void
_eventd_http_websocket_client_closed(EventdHttpWebsocketClient *self, SoupWebsocketConnection *connection)
{
    GHashTableIter iter;
    gchar *category, *key;
    GList *link, *list;
    g_hash_table_iter_init(&iter, self->subscriptions);
    while ( g_hash_table_iter_next(&iter, (gpointer *) &category, (gpointer *) &link) )
    {
        g_hash_table_iter_steal(&iter);

        g_hash_table_lookup_extended(self->context->subscribe_categories, category, (gpointer *) &key, (gpointer *) &list);
        g_hash_table_steal(self->context->subscribe_categories, category);

        list = g_list_delete_link(list, link);

        g_hash_table_insert(self->context->subscribe_categories, key, list);
    }
    g_hash_table_unref(self->subscriptions);

    if ( self->subscribe_all != NULL )
        self->context->subscribe_all = g_list_delete_link(self->context->subscribe_all, self->subscribe_all);

    if ( self->link != NULL )
        self->context->clients = g_list_remove_link(self->context->clients, self->link);

    if ( self->connection != NULL )
        g_object_unref(self->connection);

    eventd_protocol_unref(self->protocol);

    g_free(self);
}

void
eventd_http_websocket_client_handler(SoupServer *server, SoupWebsocketConnection *connection, const char *path, SoupClientContext *client, gpointer user_data)
{
    EventdPluginContext *context = user_data;

    EventdHttpWebsocketClient *self;

    self = g_new0(EventdHttpWebsocketClient, 1);
    self->context = context;

    self->protocol = eventd_protocol_evp_new(&_eventd_http_websocket_client_protocol_callbacks, self, NULL);
    self->subscriptions = g_hash_table_new(g_str_hash, g_str_equal);

    self->connection = g_object_ref(connection);
    g_signal_connect_swapped(self->connection, "message", G_CALLBACK(_eventd_http_websocket_client_message), self);
    g_signal_connect_swapped(self->connection, "closed", G_CALLBACK(_eventd_http_websocket_client_closed), self);

    self->link = context->clients = g_list_prepend(context->clients, self);
}

void
eventd_http_websocket_client_disconnect(gpointer data)
{
    EventdHttpWebsocketClient *self = data;

    g_hash_table_steal_all(self->subscriptions);
    self->subscribe_all = NULL;
    self->link = NULL;

    soup_websocket_connection_close(self->connection, SOUP_WEBSOCKET_CLOSE_NORMAL, NULL);
}

void
eventd_http_websocket_client_event_dispatch(EventdHttpWebsocketClient *self, EventdEvent *event)
{
    if ( self->current == event )
        /* Do not send back our own events */
        return;

    _eventd_http_websocket_client_send_message(self, eventd_protocol_generate_event(self->protocol, event));
}
