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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <libeventd-event.h>
#include <libeventd-event-private.h>
#include <eventd-plugin.h>
#include <libeventd-evp.h>

#include "server.h"

struct _EventdRelayServer {
    GSocketConnectable *address;
    LibeventdEvpContext *evp;
    GHashTable *events;
    guint64 tries;
    guint connection_timeout_id;
};

typedef struct {
    EventdRelayServer *server;
    EventdEvent *event;
    const gchar *id;
    gulong answered_handler;
    gulong ended_handler;
} EventdRelayEvent;

static gpointer
_eventd_relay_get_event(gpointer data, LibeventdEvpContext *evp, const gchar *id)
{
    EventdRelayServer *server = data;

    return g_hash_table_lookup(server->events, id);
}

static void
_eventd_relay_error(gpointer data, LibeventdEvpContext *evp, GError *error)
{
    g_warning("Connection error: %s", error->message);
    g_error_free(error);
}

static void
_eventd_relay_event_answered(EventdEvent *event, const gchar *answer, gpointer user_data)
{
    EventdRelayEvent *relay_event = user_data;
    EventdRelayServer *server = relay_event->server;
    GError *error = NULL;

    if ( ! libeventd_evp_context_send_answered(server->evp, relay_event->id, answer, eventd_event_get_all_answer_data(relay_event->event), &error) )
    {
        g_warning("Couldn't send ANSWERED message: %s", error->message);
        g_clear_error(&error);
    }
    g_signal_handler_disconnect(relay_event->event, relay_event->answered_handler);
    relay_event->answered_handler = 0;
}

static void
_eventd_relay_event_ended(EventdEvent *event, EventdEventEndReason reason, gpointer user_data)
{
    EventdRelayEvent *relay_event = user_data;
    EventdRelayServer *server = relay_event->server;
    GError *error = NULL;

    if ( ! libeventd_evp_context_send_ended(server->evp, relay_event->id, reason, &error) )
    {
        g_warning("Couldn't send ENDED message: %s", error->message);
        g_clear_error(&error);
    }
    g_signal_handler_disconnect(relay_event->event, relay_event->ended_handler);
    relay_event->ended_handler = 0;

    g_hash_table_remove(server->events, relay_event->id);
}

static void
_eventd_relay_answered(gpointer data, LibeventdEvpContext *evp, gpointer data_event, const gchar *answer, GHashTable *data_hash)
{
    EventdRelayEvent *relay_event = data_event;

    eventd_event_set_all_answer_data(relay_event->event, g_hash_table_ref(data_hash));
    g_signal_handler_disconnect(relay_event->event, relay_event->answered_handler);
    relay_event->answered_handler = 0;
    eventd_event_answer(relay_event->event, answer);
}

static void
_eventd_relay_ended(gpointer data, LibeventdEvpContext *evp, gpointer data_event, EventdEventEndReason reason)
{
    EventdRelayServer *server = data;
    EventdRelayEvent *relay_event = data_event;

    g_signal_handler_disconnect(relay_event->event, relay_event->ended_handler);
    relay_event->ended_handler = 0;
    eventd_event_end(relay_event->event, reason);
    g_hash_table_remove(server->events, relay_event->id);
}

static void
_eventd_relay_bye(gpointer data, LibeventdEvpContext *evp)
{
    /* TODO: check this one
    EventdRelayServer *server = data;

    libeventd_evp_context_free(server->evp);
    g_hash_table_unref(server->events);

    g_free(server);
    */
}

static void
_eventd_relay_event_free(gpointer data)
{
    EventdRelayEvent *relay_event = data;

    if ( relay_event->answered_handler > 0 )
        g_signal_handler_disconnect(relay_event->event, relay_event->answered_handler);
    if ( relay_event->ended_handler > 0 )
        g_signal_handler_disconnect(relay_event->event, relay_event->ended_handler);

    g_object_unref(relay_event->event);

    g_free(relay_event);
}

static LibeventdEvpClientInterface _eventd_relay_interface = {
    .get_event = _eventd_relay_get_event,
    .error     = _eventd_relay_error,

    .answered  = _eventd_relay_answered,
    .ended     = _eventd_relay_ended,

    .bye       = _eventd_relay_bye
};

static gboolean
_eventd_relay_reconnect(gpointer user_data)
{
    EventdRelayServer *server = user_data;

    server->connection_timeout_id = 0;
    eventd_relay_server_start(server);

    return FALSE;
}

static void
_eventd_relay_hello_handler(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    EventdRelayServer *server = user_data;
    GError *error = NULL;
    if ( ! libeventd_evp_context_send_hello_finish(server->evp, res, &error) )
    {
        g_warning("Error while sending handshake; %s", error->message);
        g_clear_error(&error);
    }
}

static void
_eventd_relay_connection_handler(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    EventdRelayServer *server = user_data;
    GSocketConnection *connection;

    connection = g_socket_client_connect_finish(G_SOCKET_CLIENT(obj), res, &error);
    if ( connection == NULL )
    {
        g_warning("Couldn't connect: %s", error->message);
        g_clear_error(&error);
        if ( ++server->tries < 4 )
            server->connection_timeout_id = g_timeout_add_seconds(3, _eventd_relay_reconnect, server);
    }
    else
    {
        libeventd_evp_context_set_connection(server->evp, connection);
        g_object_unref(connection);
        libeventd_evp_context_send_hello(server->evp, PACKAGE_NAME, _eventd_relay_hello_handler, server);
        libeventd_evp_context_receive_loop_client(server->evp, G_PRIORITY_DEFAULT);
    }
}

EventdRelayServer *
eventd_relay_server_new(void)
{
    EventdRelayServer *server;

    server = g_new0(EventdRelayServer, 1);

    server->evp = libeventd_evp_context_new(server, &_eventd_relay_interface);
    server->events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, _eventd_relay_event_free);

    return server;
}

EventdRelayServer *
eventd_relay_server_new_for_host_and_port(const gchar *host_and_port)
{
    GSocketConnectable *address;
    GError *error = NULL;

    address = libeventd_evp_get_address(host_and_port, &error);
    if ( address == NULL )
    {
        g_warning("Couldn't get address for relay server '%s': %s", host_and_port, error->message);
        g_clear_error(&error);
        return NULL;
    }

    EventdRelayServer *server;

    server = eventd_relay_server_new();
    eventd_relay_server_set_address(server, address);

    return server;
}

void
eventd_relay_server_set_address(EventdRelayServer *server, GSocketConnectable *address)
{
    if ( server->address != NULL )
        g_object_unref(server->address);
    server->address = address;
}

gboolean
eventd_relay_server_has_address(EventdRelayServer *server)
{
    return ( server->address != NULL );
}

void
eventd_relay_server_start(EventdRelayServer *server)
{
    if ( server->address == NULL )
        return;

    GSocketClient *client;

    client = g_socket_client_new();

    g_socket_client_connect_async(client, server->address, NULL, _eventd_relay_connection_handler, server);

    g_object_unref(client);
}

static void
_eventd_relay_close_handler(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    EventdRelayServer *server = user_data;

    libeventd_evp_context_close_finish(server->evp, res);
}

void
eventd_relay_server_stop(EventdRelayServer *server)
{
    if ( server->connection_timeout_id > 0 )
    {
        g_source_remove(server->connection_timeout_id);
        server->connection_timeout_id = 0;
    }

    if ( libeventd_evp_context_is_connected(server->evp, NULL) )
        libeventd_evp_context_send_bye(server->evp);
    libeventd_evp_context_close(server->evp, _eventd_relay_close_handler, server);
}

static void
_eventd_relay_event_handler(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    EventdRelayEvent *relay_event = user_data;
    EventdRelayServer *server = relay_event->server;
    const gchar *id;

    id = libeventd_evp_context_send_event_finish(server->evp, res, &error);
    if ( error != NULL )
    {
        g_warning("Couldn't send event: %s", error->message);
        g_clear_error(&error);
        server->connection_timeout_id = g_timeout_add_seconds(1, _eventd_relay_reconnect, server);
    }

    gchar *rid;

    rid = g_strdup(id);

    g_hash_table_insert(server->events, rid, relay_event);
    relay_event->id = rid;
    relay_event->answered_handler = g_signal_connect(relay_event->event, "answered", G_CALLBACK(_eventd_relay_event_answered), relay_event);
    relay_event->ended_handler = g_signal_connect(relay_event->event, "ended", G_CALLBACK(_eventd_relay_event_ended), relay_event);
}

void
eventd_relay_server_event(EventdRelayServer *server, EventdEvent *event)
{
    GError *error = NULL;
    if ( ! libeventd_evp_context_is_connected(server->evp, &error) )
    {
        if ( error != NULL )
        {
            g_warning("Couldn't send event: %s", error->message);
            g_clear_error(&error);
            server->connection_timeout_id = g_timeout_add_seconds(1, _eventd_relay_reconnect, server);
        }
        return;
    }

    EventdRelayEvent *relay_event;

    relay_event = g_new0(EventdRelayEvent, 1);
    relay_event->server = server;
    relay_event->event = g_object_ref(event);

    libeventd_evp_context_send_event(server->evp, event, _eventd_relay_event_handler, relay_event);
}

void
eventd_relay_server_free(gpointer data)
{
    EventdRelayServer *server = data;

    if ( server->connection_timeout_id > 0 )
        g_source_remove(server->connection_timeout_id);

    if ( server->evp != NULL )
        libeventd_evp_context_free(server->evp);

    g_hash_table_unref(server->events);

    g_free(server);
}
