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

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <glib.h>
#include <glib-object.h>
#include <libsoup/soup.h>

#include <libeventd-event.h>
#include <libeventd-protocol.h>
#include <libeventd-protocol-json.h>
#include <eventd-plugin.h>
#include <libeventd-helpers-config.h>

#include "http.h"
#include "websocket.h"

typedef enum {
    EVENTD_HTTP_WEBSOCKET_CLIENT_TYPE_EVP,
    EVENTD_HTTP_WEBSOCKET_CLIENT_TYPE_JSON,
} EventdHttpWebsocketClientType;

struct _EventdHttpWebsocketClient {
    EventdPluginContext *context;
    GList *link;
    EventdHttpWebsocketClientType type;
    EventdProtocol *protocol;
    SoupWebsocketConnection *connection;
    GHashTable *events;
    GList *subscribe_all;
    GHashTable *subscriptions;
};

typedef struct {
    EventdEvent *event;
    gulong answered;
    gulong ended;
} EventdHttpEventHandlers;

static void
_eventd_http_websocket_client_send_message(EventdHttpWebsocketClient *self, gchar *message)
{
    switch ( self->type )
    {
    case EVENTD_HTTP_WEBSOCKET_CLIENT_TYPE_EVP:
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
    }
    break;
    case EVENTD_HTTP_WEBSOCKET_CLIENT_TYPE_JSON:
        soup_websocket_connection_send_text(self->connection, message);
    break;
    }
    g_free(message);
}

static void
_eventd_http_websocket_client_event_answered(EventdHttpWebsocketClient *self, const gchar *answer, EventdEvent *event)
{
    EventdHttpEventHandlers *handlers;
    handlers = g_hash_table_lookup(self->events, event);
    g_return_if_fail(handlers != NULL);

    g_signal_handler_disconnect(event, handlers->answered);
    handlers->answered = 0;
    _eventd_http_websocket_client_send_message(self, eventd_protocol_generate_answered(self->protocol, event, answer));
}

static void
_eventd_http_websocket_client_protocol_answered(EventdHttpWebsocketClient *self, EventdEvent *event, const gchar *answer, EventdProtocol *protocol)
{
    EventdHttpEventHandlers *handlers;
    handlers = g_hash_table_lookup(self->events, event);
    g_return_if_fail(handlers != NULL);

    g_signal_handler_disconnect(event, handlers->answered);
    handlers->answered = 0;
    eventd_event_answer(event, answer);
}

static void
_eventd_http_websocket_client_event_ended(EventdHttpWebsocketClient *self, EventdEventEndReason reason, EventdEvent *event)
{
    g_hash_table_remove(self->events, event);
    _eventd_http_websocket_client_send_message(self, eventd_protocol_generate_ended(self->protocol, event, reason));
}

static void
_eventd_http_websocket_client_protocol_ended(EventdHttpWebsocketClient *self, EventdEvent *event, EventdEventEndReason reason, EventdProtocol *protocol)
{
    g_hash_table_remove(self->events, event);
    eventd_event_end(event, reason);
}

static void
_eventd_http_websocket_client_handle_event(EventdHttpWebsocketClient *self, EventdEvent *event)
{
    EventdHttpEventHandlers *handlers;

    handlers = g_slice_new(EventdHttpEventHandlers);
    handlers->event = g_object_ref(event);

    handlers->answered = g_signal_connect_swapped(event, "answered", G_CALLBACK(_eventd_http_websocket_client_event_answered), self);
    handlers->ended = g_signal_connect_swapped(event, "ended", G_CALLBACK(_eventd_http_websocket_client_event_ended), self);

    g_hash_table_insert(self->events, event, handlers);
}

static void
_eventd_http_websocket_client_protocol_event(EventdHttpWebsocketClient *self, EventdEvent *event, EventdProtocol *protocol)
{
#ifdef EVENTD_DEBUG
    g_debug("Received an event (category: %s): %s", eventd_event_get_category(event), eventd_event_get_name(event));
#endif /* EVENTD_DEBUG */

    if ( ! eventd_plugin_core_push_event(self->context->core, self->context->core_interface, event) )
    {
        _eventd_http_websocket_client_send_message(self, eventd_protocol_generate_ended(self->protocol, event, EVENTD_EVENT_END_REASON_DISCARD));
        return;
    }

    _eventd_http_websocket_client_handle_event(self, event);
}

static void
_eventd_http_websocket_client_protocol_passive(EventdHttpWebsocketClient *self, EventdProtocol *protocol)
{
    soup_websocket_connection_close(self->connection, SOUP_WEBSOCKET_CLOSE_PROTOCOL_ERROR, "PASSIVE not supported for WebSockets");
}

static void
_eventd_http_websocket_client_protocol_subscribe(EventdHttpWebsocketClient *self, GHashTable *categories, EventdProtocol *protocol)
{
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
_eventd_http_websocket_client_protocol_bye(EventdHttpWebsocketClient *self, EventdProtocol *protocol)
{
#ifdef EVENTD_DEBUG
    g_debug("Client connection closed");
#endif /* EVENTD_DEBUG */

    soup_websocket_connection_close(self->connection, SOUP_WEBSOCKET_CLOSE_PROTOCOL_ERROR, "BYE not supported for WebSockets");
}

static void
_eventd_http_websocket_client_event_handlers_free(gpointer data)
{
    EventdHttpEventHandlers *handlers = data;

    if ( handlers->answered > 0 )
        g_signal_handler_disconnect(handlers->event, handlers->answered);
    g_signal_handler_disconnect(handlers->event, handlers->ended);

    g_object_unref(handlers->event);

    g_slice_free(EventdHttpEventHandlers, handlers);
}

static void
_eventd_http_websocket_client_message(EventdHttpWebsocketClient *self, gint type, GBytes *message, SoupWebsocketConnection *connection)
{
    GError *error = NULL;

    if ( type != SOUP_WEBSOCKET_DATA_TEXT )
        soup_websocket_connection_close(self->connection, SOUP_WEBSOCKET_CLOSE_UNSUPPORTED_DATA, "Data must be UTF-8 text");
    else
    {
        gconstpointer data;
        gsize length;
        gchar *line;
        data = g_bytes_get_data(message, &length);
        line = g_strndup(data, length);
        if ( ! eventd_protocol_parse(self->protocol, &line, &error) )
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

    g_hash_table_unref(self->events);

    if ( self->connection != NULL )
        g_object_unref(self->connection);

    g_object_unref(self->protocol);

    g_free(self);
}

void
eventd_http_websocket_client_handler(SoupServer *server, SoupWebsocketConnection *connection, const char *path, SoupClientContext *client, gpointer user_data)
{
    EventdPluginContext *context = user_data;
    const gchar *protocol;
    EventdHttpWebsocketClientType type = EVENTD_HTTP_WEBSOCKET_CLIENT_TYPE_EVP;

    protocol = soup_websocket_connection_get_protocol(connection);
    if ( g_strcmp0(protocol, "evp-json") == 0 )
        type = EVENTD_HTTP_WEBSOCKET_CLIENT_TYPE_JSON;

    EventdHttpWebsocketClient *self;

    self = g_new0(EventdHttpWebsocketClient, 1);
    self->context = context;
    self->type = type;

    switch ( self->type )
    {
    case EVENTD_HTTP_WEBSOCKET_CLIENT_TYPE_EVP:
        self->protocol = eventd_protocol_evp_new();
    break;
    case EVENTD_HTTP_WEBSOCKET_CLIENT_TYPE_JSON:
        self->protocol = eventd_protocol_json_new();
    break;
    }
    self->events = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, _eventd_http_websocket_client_event_handlers_free);
    self->subscriptions = g_hash_table_new(g_str_hash, g_str_equal);

    self->connection = g_object_ref(connection);
    g_signal_connect_swapped(self->connection, "message", G_CALLBACK(_eventd_http_websocket_client_message), self);
    g_signal_connect_swapped(self->connection, "closed", G_CALLBACK(_eventd_http_websocket_client_closed), self);

    g_signal_connect_swapped(self->protocol, "event", G_CALLBACK(_eventd_http_websocket_client_protocol_event), self);
    g_signal_connect_swapped(self->protocol, "answered", G_CALLBACK(_eventd_http_websocket_client_protocol_answered), self);
    g_signal_connect_swapped(self->protocol, "ended", G_CALLBACK(_eventd_http_websocket_client_protocol_ended), self);
    g_signal_connect_swapped(self->protocol, "passive", G_CALLBACK(_eventd_http_websocket_client_protocol_passive), self);
    g_signal_connect_swapped(self->protocol, "subscribe", G_CALLBACK(_eventd_http_websocket_client_protocol_subscribe), self);
    g_signal_connect_swapped(self->protocol, "bye", G_CALLBACK(_eventd_http_websocket_client_protocol_bye), self);

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
    if ( g_hash_table_contains(self->events, event) )
        /* Do not send back our own events */
        return;

    _eventd_http_websocket_client_send_message(self, eventd_protocol_generate_event(self->protocol, event));
    _eventd_http_websocket_client_handle_event(self, event);
}
