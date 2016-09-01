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
#include <gio/gio.h>
#ifdef G_OS_UNIX
#include <gio/gunixsocketaddress.h>
#else /* ! G_OS_UNIX */
#define G_IS_UNIX_SOCKET_ADDRESS(a) (FALSE)
#endif /* ! G_OS_UNIX */

#include <libsoup/soup.h>

#include "libeventd-protocol.h"
#include <libeventc.h>

#include "eventd-ws-module.h"

struct _EventdWsConnection {
    gpointer data;
    GDestroyNotify disconnect_callback;
    gboolean client;
    EventdProtocol *protocol;
    GCancellable *cancellable;
    GTask *task;
    GIOStream *stream;
    GDataInputStream *in;
    GString *header;
    SoupMessage *msg;
    SoupWebsocketConnection *connection;
};

static gboolean
_eventd_ws_connection_write_message(EventdWsConnection *self, gboolean request, GError **error)
{
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    GOutputStream *out = g_io_stream_get_output_stream(self->stream);
    SoupMessage *msg = self->msg;

    gsize l = strlen("HTTP/1.1  \r\n") + 1;
    gchar *start_line;
    if ( request )
    {
        l += strlen(msg->method) + strlen(soup_uri_get_path(soup_message_get_uri(msg)));
        start_line = g_alloca(l);
        g_snprintf(start_line, l, "%s %s HTTP/1.1\r\n", msg->method, soup_uri_get_path(soup_message_get_uri(msg)));
    }
    else
    {
        l += 3 /* status code */ + strlen(msg->reason_phrase);
        start_line = g_alloca(l);
        g_snprintf(start_line, l, "HTTP/1.1 %u %s\r\n", msg->status_code, msg->reason_phrase);
    }

    if ( ! g_output_stream_write_all(out, start_line, l - 1, NULL, NULL, error) )
        return FALSE;
    SoupMessageHeadersIter iter;
    SoupMessageHeaders *headers = request ? msg->request_headers : msg->response_headers;
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
        buffer = soup_message_body_flatten(request ? msg->request_body : msg->response_body);
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

static void
_eventd_ws_connection_closed(EventdWsConnection *self, SoupWebsocketConnection *c)
{
    self->disconnect_callback(self->data);
}

static void
_eventd_ws_connection_set_connection(EventdWsConnection *self, SoupWebsocketConnectionType type)
{
    self->connection = soup_websocket_connection_new(self->stream, soup_message_get_uri(self->msg), type, NULL, EVP_SERVICE_NAME);
    g_signal_connect_swapped(self->connection, "message", G_CALLBACK(_eventd_ws_connection_message), self);
    g_signal_connect_swapped(self->connection, "error", G_CALLBACK(_eventd_ws_connection_error), self);
    g_signal_connect_swapped(self->connection, "closed", G_CALLBACK(_eventd_ws_connection_closed), self);
}

static gboolean
_eventd_ws_connection_client_send_handshake(EventdWsConnection *self, GSocketConnectable *server_identity, GError **error)
{
    const gchar *host = NULL;
    gchar *host_ = NULL;

    if ( G_IS_UNIX_SOCKET_ADDRESS(server_identity) )
        host = "localhost";
    else if ( G_IS_NETWORK_SERVICE(server_identity) )
        host = g_network_service_get_domain(G_NETWORK_SERVICE(server_identity));
    else if ( G_IS_NETWORK_ADDRESS(server_identity) )
        host = host_ = g_strdup_printf("%s:%u", g_network_address_get_hostname(G_NETWORK_ADDRESS(server_identity)),  g_network_address_get_port(G_NETWORK_ADDRESS(server_identity)));
    else if ( G_IS_PROXY_ADDRESS(server_identity) )
        host = host_ = g_strdup_printf("%s:%u", g_proxy_address_get_destination_hostname(G_PROXY_ADDRESS(server_identity)),  g_proxy_address_get_destination_port(G_PROXY_ADDRESS(server_identity)));
    else if ( G_IS_INET_SOCKET_ADDRESS(server_identity) )
    {
        GInetAddress *inet_address;
        gchar *tmp;
        inet_address = g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(server_identity));
        tmp = g_inet_address_to_string(inet_address);
        host = host_ = g_strdup_printf("%s:%u", tmp,  g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(server_identity)));
        g_free(tmp);
    }

    if ( host == NULL )
    {
        g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_HOSTNAME, "Could not get an HTTP hostname from eventc host");
        g_free(host_);
        return FALSE;
    }

    SoupURI *uri;
    uri = soup_uri_new(NULL);
    soup_uri_set_host(uri, host);
    soup_uri_set_path(uri, "/evp");
    soup_uri_set_scheme(uri, G_IS_TLS_CONNECTION(self->stream) ? "wss" : "ws");
    g_free(host_);

    self->msg = soup_message_new_from_uri(SOUP_METHOD_GET, uri);
    soup_uri_free(uri);

    gchar *protocols[] = { EVP_SERVICE_NAME, NULL };
    soup_websocket_client_prepare_handshake(self->msg, NULL, protocols);

    return _eventd_ws_connection_write_message(self, TRUE, error);
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
    g_string_free(self->header, TRUE);
    self->header = NULL;

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

    _eventd_ws_connection_set_connection(self, SOUP_WEBSOCKET_CONNECTION_CLIENT);
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
_eventd_ws_connection_handshake_server(EventdWsConnection *self)
{
    gchar response[255];
    SoupMessageHeaders *headers;
    gchar *method;
    gchar *path;
    headers = soup_message_headers_new(SOUP_MESSAGE_HEADERS_REQUEST);

    SoupStatus status;
    status = soup_headers_parse_request(self->header->str, self->header->len, headers, &method, &path, NULL);
    g_string_free(self->header, TRUE);
    self->header = NULL;

    if ( status != SOUP_STATUS_OK )
    {
        g_warning("Parsing headers error: %s", soup_status_get_phrase(status));
        g_snprintf(response, sizeof(response), "HTTP/1.1 %u %s\r\n\r\n", status, soup_status_get_phrase(status));
        g_output_stream_write_all(g_io_stream_get_output_stream(self->stream), response, strlen(response), NULL, NULL, NULL);
        soup_message_headers_free(headers);
        g_free(path);
        g_free(method);
        goto end;
    }

    SoupURI *uri;
    uri = soup_uri_new(NULL);
    soup_uri_set_host(uri, soup_message_headers_get_one(headers, "Host"));
    soup_uri_set_path(uri, path);
    soup_uri_set_scheme(uri, G_IS_TLS_CONNECTION(self->stream) ? "wss" : "ws");

    self->msg = soup_message_new_from_uri(method, uri);
    soup_uri_free(uri);
    soup_message_headers_free(self->msg->request_headers);
    self->msg->request_headers = headers;

    g_free(path);
    g_free(method);

    gboolean good_handshake, write_ok;
    gchar *protocols[] = { EVP_SERVICE_NAME, NULL };
    good_handshake = soup_websocket_server_process_handshake(self->msg, NULL, protocols);

    write_ok = _eventd_ws_connection_write_message(self, FALSE, NULL);

    if ( good_handshake && write_ok )
        _eventd_ws_connection_set_connection(self, SOUP_WEBSOCKET_CONNECTION_SERVER);

    g_object_unref(self->msg);
    self->msg = NULL;

    if ( ! write_ok )
end:
        self->disconnect_callback(self->data);
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
            if ( self->client )
                response = eventd_protocol_generate_bye(self->protocol, error->message);
            else
                response = g_strdup_printf("HTTP/1.1 500 %s\r\n\r\n", error->message);
            g_output_stream_write_all(g_io_stream_get_output_stream(self->stream), response, strlen(response), NULL, NULL, NULL);
            if ( self->task != NULL )
                g_task_return_error(self->task, error);
            g_free(response);
        }
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
        if ( self->client )
            _eventd_ws_connection_handshake_client(self);
        else
            _eventd_ws_connection_handshake_server(self);
    }
    g_free(line);
}

static EventdWsConnection *
_eventd_ws_connection_server_new(gpointer data, GDestroyNotify disconnect_callback, GCancellable *cancellable, GIOStream *stream, GDataInputStream *input, EventdProtocol *protocol, const gchar *line)
{
    EventdWsConnection *self;
    self = g_new0(EventdWsConnection, 1);
    self->data = data;
    self->disconnect_callback = disconnect_callback;
    self->client = FALSE;
    self->cancellable = cancellable;
    self->stream = stream;
    self->in = input;
    self->protocol = protocol;
    self->header = g_string_append_c(g_string_new(line), '\n');

    g_data_input_stream_set_newline_type(self->in, G_DATA_STREAM_NEWLINE_TYPE_CR_LF);
    g_filter_input_stream_set_close_base_stream(G_FILTER_INPUT_STREAM(self->in), FALSE);
    g_data_input_stream_read_line_async(self->in, G_PRIORITY_DEFAULT, self->cancellable, _eventd_ws_connection_read_callback, self);

    return self;
}

static EventdWsConnection *
_eventd_ws_connection_client_new(gpointer data, GDestroyNotify disconnect_callback, GCancellable *cancellable, GIOStream *stream, EventdProtocol *protocol)
{
    EventdWsConnection *self;
    self = g_new0(EventdWsConnection, 1);
    self->data = data;
    self->disconnect_callback = disconnect_callback;
    self->client = TRUE;
    self->cancellable = cancellable;
    self->stream = stream;
    self->in = g_data_input_stream_new(g_io_stream_get_input_stream(self->stream));
    self->protocol = protocol;
    self->header = g_string_new("");

    g_data_input_stream_set_newline_type(self->in, G_DATA_STREAM_NEWLINE_TYPE_CR_LF);
    g_filter_input_stream_set_close_base_stream(G_FILTER_INPUT_STREAM(self->in), FALSE);

    return self;
}

static void
_eventd_ws_connection_client_connect(EventdWsConnection *self, GSocketConnectable *server_identity, GAsyncReadyCallback callback, gpointer user_data)
{
    GError *error = NULL;
    self->task = g_task_new(NULL, self->cancellable, callback, user_data);
    if ( ! _eventd_ws_connection_client_send_handshake(self, server_identity, &error) )
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
_eventd_ws_connection_client_connect_sync(EventdWsConnection *self, GSocketConnectable *server_identity, GError **error)
{
    if ( ! _eventd_ws_connection_client_send_handshake(self, server_identity, error) )
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
        return FALSE;
    g_object_unref(self->in);
    self->in = NULL;

    return _eventd_ws_connection_client_check_handshake(self, error);
}

static void
_eventd_ws_connection_free(EventdWsConnection *self)
{
    if ( self->connection != NULL )
        g_object_unref(self->connection);

    if ( self->in != NULL )
        g_object_unref(self->in);

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
    module->connection_server_new = _eventd_ws_connection_server_new;
    module->connection_client_new = _eventd_ws_connection_client_new;
    module->connection_free = _eventd_ws_connection_free;

    module->connection_client_connect = _eventd_ws_connection_client_connect;
    module->connection_client_connect_finish = _eventd_ws_connection_client_connect_finish;
    module->connection_client_connect_sync = _eventd_ws_connection_client_connect_sync;

    module->connection_send_message = _eventd_ws_connection_send_message;
    module->connection_close = _eventd_ws_connection_close;
}
