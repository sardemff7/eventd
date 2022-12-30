/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2021 Quentin "Sardem FF7" Glidic
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

#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <libsoup/soup.h>

#include "libeventd-protocol.h"
#include "libeventc.h"

#include "eventd-ws-module.h"

struct _EventdWsConnection {
    gpointer data;
    GDestroyNotify disconnect_callback;
    GUri *uri;
    EventdProtocol *protocol;
    GCancellable *cancellable;
    GTask *task;
    SoupSession *session;
    SoupWebsocketConnection *connection;
};

static GSocketConnectable *
_eventd_ws_uri_parse(const gchar *uri, EventdWsUri **ws_uri, GError **error)
{
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    GUri *soup_uri;
    GSocketConnectable *address;

    soup_uri = g_uri_parse(uri, G_URI_FLAGS_HAS_PASSWORD, error);
    if ( soup_uri == NULL )
        return NULL;
    address = g_network_address_new(g_uri_get_host(soup_uri), g_uri_get_port(soup_uri));
    *ws_uri = soup_uri;

    return address;
}

static gboolean
_eventd_ws_uri_is_tls(EventdWsUri *uri)
{
    GUri *soup_uri = uri;
    return ( g_strcmp0(g_uri_get_scheme(soup_uri), "wss") == 0 );
}

static void
_eventd_ws_connection_message(EventdWsConnection *self, gint type, GBytes *message, SoupWebsocketConnection *wsc)
{
    if ( soup_websocket_connection_get_state(self->connection) != SOUP_WEBSOCKET_STATE_OPEN )
        return;

    if ( type != SOUP_WEBSOCKET_DATA_TEXT )
    {
        soup_websocket_connection_close(self->connection, SOUP_WEBSOCKET_CLOSE_UNSUPPORTED_DATA, "Data must be UTF-8 text");
        return;
    }

    GError *error = NULL;
    gchar *data;
    gsize length;
    /* We take over the data */
    data = g_bytes_unref_to_data(g_bytes_ref(message), &length);
    if ( data[length - 1] != '\n' )
    {
        soup_websocket_connection_close(self->connection, SOUP_WEBSOCKET_CLOSE_PROTOCOL_ERROR, "Malformed message: missing ending new line");
        return;
    }

    if ( ! eventd_protocol_parse(self->protocol, data, &error) )
    {
        g_warning("Parse error: %s", error->message);
        soup_websocket_connection_close(self->connection, SOUP_WEBSOCKET_CLOSE_PROTOCOL_ERROR, error->message);
    }
    g_clear_error(&error);
}

static void
_eventd_ws_connection_error(EventdWsConnection *self, GError *error, SoupWebsocketConnection *c)
{
    g_warning("WebSocket error: %s", error->message);
}

static gboolean
_eventd_ws_connection_delayed_unref(gpointer user_data)
{
    g_object_unref(user_data);
    return G_SOURCE_REMOVE;
}

static void
_eventd_ws_connection_closed(EventdWsConnection *self, SoupWebsocketConnection *c)
{
    /*
     * Unref’ing the connection from the "closed" handler leads to a crash
     * so we delay it to an idle callback.
     */
    g_idle_add(_eventd_ws_connection_delayed_unref, self->connection);
    self->connection = NULL;
    self->disconnect_callback(self->data);
}

static EventdWsConnection *
_eventd_ws_connection_client_new(gpointer data, EventdWsUri *uri, GDestroyNotify disconnect_callback, GCancellable *cancellable, EventdProtocol *protocol)
{
    EventdWsConnection *self;
    self = g_new0(EventdWsConnection, 1);
    self->data = data;
    self->disconnect_callback = disconnect_callback;
    self->uri = uri;
    self->cancellable = cancellable;
    self->protocol = protocol;
    self->session = soup_session_new();

    return self;
}

static void
_eventd_ws_connection_client_connect_callback(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    EventdWsConnection *self = user_data;
    GError *error = NULL;

    self->connection = soup_session_websocket_connect_finish(self->session, res, &error);
    if ( self->connection == NULL )
    {
        g_task_return_error(self->task, error);
        return;
    }


    g_signal_connect_swapped(self->connection, "message", G_CALLBACK(_eventd_ws_connection_message), self);
    g_signal_connect_swapped(self->connection, "error", G_CALLBACK(_eventd_ws_connection_error), self);
    g_signal_connect_swapped(self->connection, "closed", G_CALLBACK(_eventd_ws_connection_closed), self);
    g_task_return_boolean(self->task, TRUE);
}

static void
_eventd_ws_connection_client_connect(EventdWsConnection *self, GAsyncReadyCallback callback, gpointer user_data)
{
    SoupMessageHeaders *headers;
    SoupMessage *msg = soup_message_new_from_uri(SOUP_METHOD_GET, self->uri);
    headers = soup_message_get_request_headers(msg);

    soup_message_headers_replace(headers, "Host", g_uri_get_host(self->uri));

    gchar *protocols[] = { EVP_SERVICE_NAME, NULL };
    self->task = g_task_new(NULL, self->cancellable, callback, user_data);
    soup_session_websocket_connect_async(self->session, msg, NULL, protocols, G_PRIORITY_DEFAULT, self->cancellable, _eventd_ws_connection_client_connect_callback, self);
}

static gboolean
_eventd_ws_connection_client_connect_finish(EventdWsConnection *self, GAsyncResult *result, GError **error)
{
    g_object_unref(self->task);
    self->task = NULL;
    return g_task_propagate_boolean(G_TASK(result), error);
}

static gboolean
_eventd_ws_connection_client_connect_sync(EventdWsConnection *self, GError **error)
{
    g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_NOSYS, "WebSocket does not support synchronous connection");
    return FALSE;
}

static gboolean
_eventd_ws_connection_client_is_connected(EventdWsConnection *self)
{
    return ( self->connection != NULL ) && ( soup_websocket_connection_get_state(self->connection) == SOUP_WEBSOCKET_STATE_OPEN );
}

static void _eventd_ws_connection_cleanup(EventdWsConnection *self);
static void
_eventd_ws_connection_free(EventdWsConnection *self)
{
    if ( self->uri != NULL )
        g_uri_unref(self->uri);

    _eventd_ws_connection_cleanup(self);

    if ( self->task != NULL )
        g_object_unref(self->task);

    g_free(self);
}

static void
_eventd_ws_connection_send_message(EventdWsConnection *self, const gchar *message)
{
    if ( self->connection == NULL )
        return;
    soup_websocket_connection_send_text(self->connection, message);
}

static void
_eventd_ws_connection_cleanup(EventdWsConnection *self)
{
    if ( self->connection != NULL )
        g_object_unref(self->connection);
    self->connection = NULL;
}

static void
_eventd_ws_connection_close(EventdWsConnection *self)
{
    if ( self->connection == NULL )
        self->disconnect_callback(self->data);
    else
        soup_websocket_connection_close(self->connection, SOUP_WEBSOCKET_CLOSE_NORMAL, NULL);
}


EVENTD_EXPORT
void
eventd_ws_module_get_info(EventdWsModule *module)
{
    module->uri_parse = _eventd_ws_uri_parse;
    module->uri_is_tls = _eventd_ws_uri_is_tls;

    module->connection_client_new = _eventd_ws_connection_client_new;
    module->connection_free = _eventd_ws_connection_free;

    module->connection_client_connect = _eventd_ws_connection_client_connect;
    module->connection_client_connect_finish = _eventd_ws_connection_client_connect_finish;
    module->connection_client_connect_sync = _eventd_ws_connection_client_connect_sync;
    module->connection_client_is_connected = _eventd_ws_connection_client_is_connected;

    module->connection_send_message = _eventd_ws_connection_send_message;
    module->connection_cleanup = _eventd_ws_connection_cleanup;
    module->connection_close = _eventd_ws_connection_close;
}
