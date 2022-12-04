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
    SoupURI *uri;
    const gchar *secret;
    EventdProtocol *protocol;
    GCancellable *cancellable;
    GTask *task;
    GIOStream *stream;
    GDataInputStream *in;
    GString *header;
    SoupMessage *msg;
    SoupWebsocketConnection *connection;
};

static GSocketConnectable *
_eventd_ws_uri_parse(const gchar *uri, EventdWsUri **ws_uri, GError **error)
{
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    SoupURI *soup_uri;
    GSocketConnectable *address;

    soup_uri = soup_uri_new(uri);
    address = g_network_address_new(soup_uri_get_host(soup_uri), soup_uri_get_port(soup_uri));
    *ws_uri = soup_uri;

    return address;
}

static gboolean
_eventd_ws_uri_is_tls(EventdWsUri *uri)
{
    SoupURI *soup_uri = uri;
    return ( soup_uri->scheme == SOUP_URI_SCHEME_WSS );
}

static gboolean
_eventd_ws_connection_write_message(EventdWsConnection *self, GError **error)
{
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    GOutputStream *out = g_io_stream_get_output_stream(self->stream);
    SoupMessage *msg = self->msg;

    gsize l = strlen("HTTP/1.1  \r\n") + 1;
    gchar *start_line;
    gchar *request_uri = soup_uri_to_string(soup_message_get_uri(msg), TRUE);
    l += strlen(msg->method) + strlen(request_uri);
    start_line = g_newa(gchar, l);
    g_snprintf(start_line, l, "%s %s HTTP/1.1\r\n", msg->method, request_uri);
    g_free(request_uri);

    if ( ! g_output_stream_write_all(out, start_line, l - 1, NULL, NULL, error) )
        return FALSE;
    SoupMessageHeadersIter iter;
    SoupMessageHeaders *headers = msg->request_headers;
    const gchar *name, *value;
    gboolean write_ok = TRUE;
    soup_message_headers_iter_init(&iter, headers);
    while ( write_ok && soup_message_headers_iter_next(&iter, &name, &value) )
    {
        write_ok = write_ok
            && g_output_stream_write_all(out, name, strlen(name), NULL, NULL, error)
            && g_output_stream_write_all(out, ": ", strlen(": "), NULL, NULL, error)
            && g_output_stream_write_all(out, value, strlen(value), NULL, NULL, error)
            && g_output_stream_write_all(out, "\r\n", strlen("\r\n"), NULL, NULL, error);
    }
    write_ok = write_ok && g_output_stream_write_all(out, "\r\n", strlen("\r\n"), NULL, NULL, error);

    if ( write_ok )
    {
        SoupBuffer *buffer;
        buffer = soup_message_body_flatten(msg->request_body);
        write_ok = g_output_stream_write_all(out, buffer->data, buffer->length, NULL, NULL, error);
        soup_buffer_free(buffer);
    }

    return write_ok;
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
    data = (gchar *) g_bytes_get_data(message, &length);
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

static gboolean
_eventd_ws_connection_client_send_handshake(EventdWsConnection *self, GError **error)
{
    self->msg = soup_message_new_from_uri(SOUP_METHOD_GET, self->uri);
    soup_message_headers_replace(self->msg->request_headers, "Host", soup_uri_get_host(self->uri));
    if ( self->secret != NULL )
        soup_message_headers_replace(self->msg->request_headers, "Eventd-Secret", self->secret);

    gchar *protocols[] = { EVP_SERVICE_NAME, NULL };
    soup_websocket_client_prepare_handshake(self->msg, NULL, protocols);

    return _eventd_ws_connection_write_message(self, error);
}

static gboolean
_eventd_ws_connection_client_check_handshake(EventdWsConnection *self, GError **error)
{
    SoupMessageHeaders *headers;
    headers = soup_message_headers_new(SOUP_MESSAGE_HEADERS_RESPONSE);

    SoupStatus status;
    gchar *reason;
    gboolean ok;
    ok = soup_headers_parse_response(self->header->str, self->header->len, headers, NULL, &status, &reason);
    g_string_truncate(self->header, 0);

    if ( ! ok )
    {
        g_set_error_literal(error, SOUP_HTTP_ERROR, status, reason);
        soup_message_headers_free(headers);
        g_free(reason);
        return FALSE;
    }
    soup_message_set_status_full(self->msg, status, reason);
    g_free(reason);

    soup_message_headers_free(self->msg->response_headers);
    self->msg->response_headers = headers;

    if ( ! soup_websocket_client_verify_handshake(self->msg, error) )
        return FALSE;

    self->connection = soup_websocket_connection_new(self->stream, soup_message_get_uri(self->msg), SOUP_WEBSOCKET_CONNECTION_CLIENT, NULL, EVP_SERVICE_NAME);
    g_signal_connect_swapped(self->connection, "message", G_CALLBACK(_eventd_ws_connection_message), self);
    g_signal_connect_swapped(self->connection, "error", G_CALLBACK(_eventd_ws_connection_error), self);
    g_signal_connect_swapped(self->connection, "closed", G_CALLBACK(_eventd_ws_connection_closed), self);
    return TRUE;
}

static void
_eventd_ws_connection_handshake_client(EventdWsConnection *self)
{
    GError *error = NULL;
    if ( ! _eventd_ws_connection_client_check_handshake(self, &error) )
        g_task_return_error(self->task, error);
    else
        g_task_return_boolean(self->task, TRUE);
}

static void
_eventd_ws_connection_read_callback(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    EventdWsConnection *self = user_data;
    GError *error = NULL;
    gchar *line;

    line = g_data_input_stream_read_line_finish_utf8(G_DATA_INPUT_STREAM(obj), res, NULL, &error);
    if ( line == NULL )
    {
        if  ( ( error != NULL ) && ( ! g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED) ) )
        {
            gchar *response;
            response = eventd_protocol_generate_bye(self->protocol, error->message);
            g_output_stream_write_all(g_io_stream_get_output_stream(self->stream), response, strlen(response), NULL, NULL, NULL);
            g_free(response);
        }
        if ( self->task != NULL )
        {
            if ( error == NULL )
                g_set_error_literal(&error, SOUP_HTTP_ERROR, SOUP_STATUS_IO_ERROR, soup_status_get_phrase(SOUP_STATUS_IO_ERROR));
            g_task_return_error(self->task, error);
        }
        else
            g_clear_error(&error);
        self->disconnect_callback(self->data);
    }
    else if ( line[0] != '\0' )
    {
        g_string_append(g_string_append(self->header, line), "\r\n");
        g_data_input_stream_read_line_async(self->in, G_PRIORITY_DEFAULT, self->cancellable, _eventd_ws_connection_read_callback, self);
    }
    else
    {
        g_object_unref(self->in);
        self->in = NULL;
        _eventd_ws_connection_handshake_client(self);
    }
    g_free(line);
}

static EventdWsConnection *
_eventd_ws_connection_client_new(gpointer data, EventdWsUri *uri, GDestroyNotify disconnect_callback, GCancellable *cancellable, EventdProtocol *protocol)
{
    EventdWsConnection *self;
    self = g_new0(EventdWsConnection, 1);
    self->data = data;
    self->disconnect_callback = disconnect_callback;
    self->uri = uri;
    if ( ( soup_uri_get_user(uri) == NULL ) || ( *soup_uri_get_user(uri) == '\0' ) )
        self->secret = soup_uri_get_password(uri);
    self->cancellable = cancellable;
    self->protocol = protocol;
    self->header = g_string_new("");

    return self;
}

static void
_eventd_ws_connection_client_connect_prepare(EventdWsConnection *self, GIOStream *stream)
{
    self->stream = stream;
    self->in = g_data_input_stream_new(g_io_stream_get_input_stream(self->stream));

    g_data_input_stream_set_newline_type(self->in, G_DATA_STREAM_NEWLINE_TYPE_CR_LF);
    g_filter_input_stream_set_close_base_stream(G_FILTER_INPUT_STREAM(self->in), FALSE);
}

static void
_eventd_ws_connection_client_connect(EventdWsConnection *self, GIOStream *stream, GAsyncReadyCallback callback, gpointer user_data)
{
    GError *error = NULL;
    _eventd_ws_connection_client_connect_prepare(self, stream);
    self->task = g_task_new(NULL, self->cancellable, callback, user_data);
    if ( ! _eventd_ws_connection_client_send_handshake(self, &error) )
        g_task_return_error(self->task, error);
    else
        g_data_input_stream_read_line_async(self->in, G_PRIORITY_DEFAULT, self->cancellable, _eventd_ws_connection_read_callback, self);
}

static gboolean
_eventd_ws_connection_client_connect_finish(EventdWsConnection *self, GAsyncResult *result, GError **error)
{
    g_object_unref(self->task);
    self->task = NULL;
    return g_task_propagate_boolean(G_TASK(result), error);
}

static gboolean
_eventd_ws_connection_client_connect_sync(EventdWsConnection *self, GIOStream *stream, GError **error)
{
    _eventd_ws_connection_client_connect_prepare(self, stream);

    if ( ! _eventd_ws_connection_client_send_handshake(self, error) )
        return FALSE;

    gchar *line;
    while ( ( line = g_data_input_stream_read_line_utf8(self->in, NULL, self->cancellable, error) ) != NULL )
    {
        if ( line[0] == '\0' )
            break;
        g_string_append(g_string_append(self->header, line), "\r\n");
        g_free(line);
    }
    g_free(line);
    if ( line == NULL )
    {
        if ( ( error != NULL ) && ( *error == NULL ) )
            g_set_error_literal(error, SOUP_HTTP_ERROR, SOUP_STATUS_IO_ERROR, soup_status_get_phrase(SOUP_STATUS_IO_ERROR));
        return FALSE;
    }
    g_object_unref(self->in);
    self->in = NULL;

    return _eventd_ws_connection_client_check_handshake(self, error);
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
        soup_uri_free(self->uri);

    _eventd_ws_connection_cleanup(self);

    if ( self->task != NULL )
        g_object_unref(self->task);

    if ( self->msg != NULL )
        g_object_unref(self->msg);

    if ( self->header != NULL )
        g_string_free(self->header, TRUE);

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

    if ( self->in != NULL )
        g_object_unref(self->in);
    self->in = NULL;
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
