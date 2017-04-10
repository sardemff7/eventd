/*
 * libeventc - Library to communicate with eventd
 *
 * Copyright © 2011-2017 Quentin "Sardem FF7" Glidic
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with eventd. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#ifdef G_OS_UNIX
#include <gio/gunixsocketaddress.h>
#define DEFAULT_SOCKET_SCHEME "unix:"
#else /* ! G_OS_UNIX */
#define G_IS_UNIX_SOCKET_ADDRESS(a) (FALSE)
#define DEFAULT_SOCKET_SCHEME "file://"
#endif /* ! G_OS_UNIX */
#include "gio-compat.h"

#include "libeventd-event.h"
#include "libeventd-protocol.h"

#include "eventd-ws-module.h"

#include "libeventc.h"

#define EVENTC_CONNECTION_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), EVENTC_TYPE_CONNECTION, EventcConnectionPrivate))

EVENTD_EXPORT GType eventc_connection_get_type(void);
G_DEFINE_TYPE(EventcConnection, eventc_connection, G_TYPE_OBJECT)


enum {
    SIGNAL_EVENT,
    SIGNAL_DISCONNECTED,
    LAST_SIGNAL
};

static EventdWsModule *_eventc_connection_ws_module = NULL;
static guint _eventc_connection_signals[LAST_SIGNAL];

struct _EventcConnectionPrivate {
    GSocketConnectable *address;
    GSocketConnectable *server_identity;
    gboolean ws_tls;
    gboolean accept_unknown_ca;
    GTlsCertificate *certificate;
    gboolean subscribe;
    GHashTable *subscriptions;
    GError *error;
    EventdProtocol* protocol;
    GCancellable *cancellable;
    GSocketConnection *connection;
    EventdWsConnection *ws;
    GDataInputStream *in;
    GDataOutputStream *out;
};

typedef struct {
    EventdEvent *event;
} EventdConnectionEventHandlers;

typedef struct {
    EventcConnection *self;
    GAsyncReadyCallback callback;
    gpointer user_data;
} EventcConnectionCallbackData;

static void _eventc_connection_close_internal(EventcConnection *self);

static GSocketConnectable *
_eventc_get_address(const gchar *uri, EventdWsUri **ws_uri, GError **error)
{
    GError *_inner_error_ = NULL;
    if ( g_str_has_prefix(uri, "unix:") )
    {
#ifdef G_OS_UNIX
        const gchar *path = uri + strlen("unix:");

        if ( g_str_has_prefix(path, "@") )
        {
            if ( g_unix_socket_address_abstract_names_supported() )
                return G_SOCKET_CONNECTABLE(g_unix_socket_address_new_with_type(path + 1, -1, G_UNIX_SOCKET_ADDRESS_ABSTRACT));
            g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_URI, "Abstract UNIX socket names are not supported");
        }
        else if ( g_file_test(path, G_FILE_TEST_EXISTS) && ( ! g_file_test(path, G_FILE_TEST_IS_DIR|G_FILE_TEST_IS_REGULAR) ) )
            return G_SOCKET_CONNECTABLE(g_unix_socket_address_new(path));
        else
            g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_URI, "File '%s' does not exist or is not a UNIX socket", path);
#else /* ! G_OS_UNIX */
        g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_URI, "UNIX sockets are not supported");
#endif /* ! G_OS_UNIX */
        return NULL;
    }

    if ( g_str_has_prefix(uri, "file://") )
    {
        const gchar *path = uri + strlen("file://");

        if ( g_file_test(path, G_FILE_TEST_IS_REGULAR) )
        {
            gchar *str;
            guint64 port;
            g_file_get_contents(path, &str, NULL, &_inner_error_);
            if ( str == NULL )
            {
                g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_URI, "Could not read file '%s': %s", path, _inner_error_->message);
                g_error_free(_inner_error_);
            }
            else
            {
                port = g_ascii_strtoull(str, NULL, 10);
                g_free(str);
                if ( ( port == 0 ) || ( port > G_MAXUINT16 ) )
                    g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_URI, "File '%s' contains wrong port '%" G_GINT64_MODIFIER "u'", path, port);
                else
                    return g_network_address_new_loopback(port);
            }
        }
        else
            g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_URI, "File '%s' does not exist", path);
        return NULL;
    }

    if ( g_str_has_prefix(uri, "ws://") || g_str_has_prefix(uri, "wss://") )
    {
        if ( _eventc_connection_ws_module == NULL )
        {
            g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_URI, "Could not load WebSocket module");
            return NULL;
        }

        return eventd_ws_uri_parse(_eventc_connection_ws_module, uri, ws_uri);
    }

    if ( g_str_has_prefix(uri, "evp://") )
    {
        const gchar *hostname = uri + strlen("evp://");
        GSocketConnectable *address;

        address = g_network_address_parse(hostname, 0, &_inner_error_);

        if ( address == NULL )
        {
            g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_URI, "Could not resolve host name '%s': %s", hostname, _inner_error_->message);
            g_error_free(_inner_error_);
        }
        else if ( g_network_address_get_port(G_NETWORK_ADDRESS(address)) == 0 )
        {
            g_object_unref(address);
            return g_network_service_new(EVP_SERVICE_NAME, EVP_TRANSPORT_NAME, hostname);
        }
        return address;
    }

    g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_URI, "Unsupported URI: %s", uri);
    return NULL;
}

/**
 * eventc_get_version:
 *
 * Retrieves the runtime-version of libeventc.
 *
 * Returns: (transfer none): the version of libeventc
 */
EVENTD_EXPORT
const gchar *
eventc_get_version(void)
{
    return EVENTD_VERSION;
}

EVENTD_EXPORT
GQuark
eventc_error_quark(void)
{
    return g_quark_from_static_string("eventc_error-quark");
}

static gboolean
_eventc_connection_send_message(EventcConnection *self, gchar *message, GError **error)
{
    gboolean r = FALSE;
    if ( self->priv->error != NULL )
    {
        g_propagate_error(error, self->priv->error);
        self->priv->error = NULL;
        goto end;
    }

    GError *_inner_error_ = NULL;

#ifdef EVENTD_DEBUG
    g_debug("Sending message:\n%s", message);
#endif /* EVENTD_DEBUG */

    if ( self->priv->ws != NULL )
    {
        eventd_ws_connection_send_message(_eventc_connection_ws_module, self->priv->ws, message);
        r = TRUE;
        goto end;
    }

    if ( g_data_output_stream_put_string(self->priv->out, message, NULL, &_inner_error_) )
        r = TRUE;
    else
    {
        g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_CONNECTION, "Failed to send message: %s", _inner_error_->message);
        g_error_free(_inner_error_);
        g_cancellable_cancel(self->priv->cancellable);
        _eventc_connection_close_internal(self);
    }

end:
    g_free(message);
    return r;
}

static void
_eventc_connection_protocol_event(EventdProtocol *protocol, EventdEvent *event, gpointer user_data)
{
    EventcConnection *self = user_data;

    g_signal_emit(self, _eventc_connection_signals[SIGNAL_EVENT], 0, event);
}

static void
_eventc_connection_protocol_bye(EventdProtocol *protocol, const gchar *message, gpointer user_data)
{
    EventcConnection *self = user_data;

    if ( self->priv->ws != NULL )
        eventd_ws_connection_close(_eventc_connection_ws_module, self->priv->ws);
    else
        _eventc_connection_close_internal(self);
}

static const EventdProtocolCallbacks _eventc_connection_protocol_callbacks = {
    .event = _eventc_connection_protocol_event,
    .bye = _eventc_connection_protocol_bye,
};

static void
_eventc_connection_read_callback(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    EventcConnection *self = user_data;
    GError *error = NULL;
    gchar *line;

    line = g_data_input_stream_read_line_finish(G_DATA_INPUT_STREAM(obj), res, NULL, &error);
    if ( line == NULL )
    {
        if ( ! g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED) )
        {
            if ( error != NULL )
                g_set_error(&self->priv->error, EVENTC_ERROR, EVENTC_ERROR_CONNECTION, "Could not read line: %s", error->message);
        }
        _eventc_connection_close_internal(self);
        g_clear_error(&error);
    }
    else if ( eventd_protocol_parse(self->priv->protocol, line, &self->priv->error) )
    {
        if ( self->priv->in != NULL )
            g_data_input_stream_read_line_async(self->priv->in, G_PRIORITY_DEFAULT, self->priv->cancellable, _eventc_connection_read_callback, self);
    }
    g_free(line);
}

static void _eventc_connection_finalize(GObject *object);

static void
eventc_connection_class_init(EventcConnectionClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(EventcConnectionPrivate));

    eventc_connection_parent_class = g_type_class_peek_parent(klass);

    object_class->finalize = _eventc_connection_finalize;

    _eventc_connection_ws_module = eventd_ws_init();

    /**
     * EventcConnection::event:
     * @connection: the #EventcConnection that the event is from
     * @event: the #EventdEvent that was received
     *
     * Emitted when receiving an event.
     */
    _eventc_connection_signals[SIGNAL_EVENT] =
        g_signal_new("event",
                     G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(EventcConnectionClass, event),
                     NULL, NULL,
                     g_cclosure_marshal_generic,
                     G_TYPE_NONE, 1, EVENTD_TYPE_EVENT);

    /**
     * EventcConnection::disconnected:
     * @connection: the #EventcConnection that was disconnected
     *
     * Emitted when the connection is closed.
     */
    _eventc_connection_signals[SIGNAL_DISCONNECTED] =
        g_signal_new("disconnected",
                     G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(EventcConnectionClass, event),
                     NULL, NULL,
                     g_cclosure_marshal_generic,
                     G_TYPE_NONE, 0);
}

static void
eventc_connection_init(EventcConnection *self)
{
    self->priv = EVENTC_CONNECTION_GET_PRIVATE(self);

    self->priv->protocol = eventd_protocol_new(&_eventc_connection_protocol_callbacks, self, NULL);
    self->priv->cancellable = g_cancellable_new();
}

static void
_eventc_connection_finalize(GObject *object)
{
    EventcConnection *self = EVENTC_CONNECTION(object);

    if ( self->priv->error != NULL )
        g_error_free(self->priv->error);

    if ( self->priv->subscriptions != NULL )
        g_hash_table_unref(self->priv->subscriptions);

    if ( self->priv->certificate != NULL )
        g_object_unref(self->priv->certificate);

    if ( self->priv->server_identity != NULL )
        g_object_unref(self->priv->server_identity);

    if ( self->priv->ws != NULL )
        eventd_ws_connection_free(_eventc_connection_ws_module, self->priv->ws);

    if ( self->priv->address != NULL )
        g_object_unref(self->priv->address);

    g_object_unref(self->priv->cancellable);
    eventd_protocol_unref(self->priv->protocol);

    G_OBJECT_CLASS(eventc_connection_parent_class)->finalize(object);
}

/**
 * eventc_connection_new:
 * @uri: (nullable): the URI for the eventd instance to connect to or %NULL for the default local socket
 * @error: (out) (optional): return location for error or %NULL to ignore
 *
 * Creates a new connection to an eventd daemon.
 * See eventc_connection_set_uri() for the exact format for @uri.
 *
 * Returns: (transfer full): a new connection, or %NULL if @uri could not be resolved
 */
EVENTD_EXPORT
EventcConnection *
eventc_connection_new(const gchar *uri, GError **error)
{
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    EventcConnection *self;

    self = g_object_new(EVENTC_TYPE_CONNECTION, NULL);

    if ( ! eventc_connection_set_uri(self, uri, error) )
    {
        g_object_unref(self);
        return NULL;
    }

    return self;
}

/**
 * eventc_connection_new_for_connectable:
 * @address: (transfer full): a #GSocketConnectable
 *
 * Creates a new connection to an eventd daemon.
 *
 * Returns: (transfer full): a new connection
 */
EVENTD_EXPORT
EventcConnection *
eventc_connection_new_for_connectable(GSocketConnectable *address)
{
    g_return_val_if_fail(G_IS_SOCKET_CONNECTABLE(address), NULL);

    EventcConnection *self;

    self = g_object_new(EVENTC_TYPE_CONNECTION, NULL);

    self->priv->address = address;

    return self;
}

/**
 * eventc_connection_is_connected:
 * @connection: an #EventcConnection
 * @error: (out) (optional): return location for error or %NULL to ignore
 *
 * Retrieves whether a given connection is actually connected to a server or
 * not.
 *
 * Returns: %TRUE if the connection was successful
 */
EVENTD_EXPORT
gboolean
eventc_connection_is_connected(EventcConnection *self, GError **error)
{
    g_return_val_if_fail(EVENTC_IS_CONNECTION(self), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if ( self->priv->error != NULL )
    {
        g_propagate_error(error, self->priv->error);
        self->priv->error = NULL;
        return FALSE;
    }

    return ( ( self->priv->connection != NULL ) && g_socket_connection_is_connected(self->priv->connection) );
}

static gboolean
_eventc_connection_expect_connected(EventcConnection *self, GError **error)
{
    GError *_inner_error_ = NULL;

    if ( eventc_connection_is_connected(self, &_inner_error_) )
        return TRUE;

    if ( _inner_error_ != NULL )
        g_propagate_error(error, _inner_error_);
    else
        g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_NOT_CONNECTED, "Not connected, you must connect first");
    return FALSE;
}

static gboolean
_eventc_connection_expect_disconnected(EventcConnection *self, GError **error)
{
    GError *_inner_error_ = NULL;

    if ( eventc_connection_is_connected(self, &_inner_error_) )
    {
        g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_ALREADY_CONNECTED, "Already connected, you must disconnect first");
        return FALSE;
    }

    if ( _inner_error_ != NULL )
    {
        g_propagate_error(error, _inner_error_);
        return FALSE;
    }

    return TRUE;
}

static gsize
_eventc_connection_tls_add_certificate_error(gchar *s, gsize o, GTlsCertificateFlags errors, GTlsCertificateFlags flag, gboolean *first)
{
    if ( ! ( errors & flag ) )
        return o;

    const gchar *error = NULL;
    const gchar *sep = "";
    switch ( flag )
    {
    case G_TLS_CERTIFICATE_UNKNOWN_CA:
        error = "unknown CA";
    break;
    case G_TLS_CERTIFICATE_BAD_IDENTITY:
        error = "bad identity";
    break;
    case G_TLS_CERTIFICATE_NOT_ACTIVATED:
        error = "not activated";
    break;
    case G_TLS_CERTIFICATE_EXPIRED:
        error = "expired";
    break;
    case G_TLS_CERTIFICATE_REVOKED:
        error = "revoked";
    break;
    case G_TLS_CERTIFICATE_INSECURE:
        error = "insecure";
    break;
    case G_TLS_CERTIFICATE_GENERIC_ERROR:
        return o;
    case G_TLS_CERTIFICATE_VALIDATE_ALL:
        g_return_val_if_reached(o);
    }

    sep = *first ? ": " : ", ";
    *first = FALSE;
    o += g_snprintf(s+o, 255-o, "%s%s", sep, error);

    return o;
}

static gboolean
_eventc_connection_tls_connection_accept_certificate(EventcConnection *self, GTlsCertificate *peer_cert, GTlsCertificateFlags errors, GTlsConnection *conn)
{
    gboolean first = TRUE;
    gsize o = 0;
    gchar s[255] = { '\0' };
    GSocketConnectable *address;
    gchar *address_string;

    o = _eventc_connection_tls_add_certificate_error(s, o, errors, G_TLS_CERTIFICATE_UNKNOWN_CA, &first);
    o = _eventc_connection_tls_add_certificate_error(s, o, errors, G_TLS_CERTIFICATE_BAD_IDENTITY, &first);
    o = _eventc_connection_tls_add_certificate_error(s, o, errors, G_TLS_CERTIFICATE_NOT_ACTIVATED, &first);
    o = _eventc_connection_tls_add_certificate_error(s, o, errors, G_TLS_CERTIFICATE_EXPIRED, &first);
    o = _eventc_connection_tls_add_certificate_error(s, o, errors, G_TLS_CERTIFICATE_REVOKED, &first);
    o = _eventc_connection_tls_add_certificate_error(s, o, errors, G_TLS_CERTIFICATE_INSECURE, &first);
    o = _eventc_connection_tls_add_certificate_error(s, o, errors, G_TLS_CERTIFICATE_GENERIC_ERROR, &first);

    g_return_val_if_fail(o < 256, FALSE);

    address = g_tls_client_connection_get_server_identity(G_TLS_CLIENT_CONNECTION(conn));

    address_string = g_socket_connectable_to_string(address);
    g_set_error(&self->priv->error, EVENTC_ERROR, EVENTC_ERROR_CONNECTION, "Failed to connect: [%s] TLS certificate error%s", address_string, s);
    g_free(address_string);

    return FALSE;
}

static void
_eventc_connection_socket_client_event(EventcConnection *self, GSocketClientEvent event, GSocketConnectable *connectable, GIOStream *connection, GSocketClient *client)
{
    GError *error = NULL;
    switch ( event )
    {
    case G_SOCKET_CLIENT_CONNECTING:
    {
        gboolean safe = TRUE;
        if ( self->priv->ws != NULL )
            safe = ! self->priv->ws_tls;
        else if ( G_IS_TCP_CONNECTION(connection) )
        {
            GSocketAddress *address;
            address = g_socket_connection_get_remote_address(G_SOCKET_CONNECTION(connection), &error);
            if ( address == NULL )
            {
                g_set_error(&self->priv->error, EVENTC_ERROR, EVENTC_ERROR_CONNECTION, "Could not get address while connecting: %s", error->message);
                g_clear_error(&error);
                g_cancellable_cancel(self->priv->cancellable);
                return;
            }

            GInetAddress *inet_address;
            inet_address = g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(address));

            safe = g_inet_address_get_is_loopback(inet_address);
        }
        g_socket_client_set_tls(client, ! safe);
    }
    break;
    case G_SOCKET_CLIENT_TLS_HANDSHAKING:
        g_signal_connect_swapped(connection, "accept-certificate", G_CALLBACK(_eventc_connection_tls_connection_accept_certificate), self);
        if ( self->priv->server_identity != NULL )
            g_tls_client_connection_set_server_identity(G_TLS_CLIENT_CONNECTION(connection), self->priv->server_identity);
        if ( self->priv->certificate != NULL )
            g_tls_connection_set_certificate(G_TLS_CONNECTION(connection), self->priv->certificate);
    break;
    default:
    break;
    }
}
static GSocketClient *
_eventc_connection_get_socket_client(EventcConnection *self)
{
    GSocketClient *client;
    client = g_socket_client_new();

    g_signal_connect_swapped(client, "event", G_CALLBACK(_eventc_connection_socket_client_event), self);

    GTlsCertificateFlags flags = G_TLS_CERTIFICATE_VALIDATE_ALL;

    if ( self->priv->accept_unknown_ca )
        flags &= ~G_TLS_CERTIFICATE_UNKNOWN_CA;

    g_socket_client_set_tls_validation_flags(client, flags);

    g_cancellable_reset(self->priv->cancellable);

    return client;
}

static gboolean
_eventc_connection_connect_check(EventcConnection *self, GError *_inner_error_, GError **error)
{
    if ( self->priv->connection != NULL )
    {
        if ( self->priv->ws == NULL )
        {
            self->priv->out = g_data_output_stream_new(g_io_stream_get_output_stream(G_IO_STREAM(self->priv->connection)));
            self->priv->in = g_data_input_stream_new(g_io_stream_get_input_stream(G_IO_STREAM(self->priv->connection)));

            g_data_input_stream_read_line_async(self->priv->in, G_PRIORITY_DEFAULT, self->priv->cancellable, _eventc_connection_read_callback, self);
        }
        return TRUE;
    }

    if ( ( _inner_error_->code == G_IO_ERROR_CANCELLED ) || ( self->priv->error != NULL ) )
    {
        g_propagate_error(error, self->priv->error);
        self->priv->error = NULL;
    }
    else
        g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_CONNECTION, "Failed to connect: %s", _inner_error_->message);
    g_error_free(_inner_error_);
    return FALSE;
}

static gboolean
_eventc_connection_connect_after(EventcConnection *self, GError **error)
{
    if ( self->priv->subscribe )
        return _eventc_connection_send_message(self, eventd_protocol_generate_subscribe(self->priv->protocol, self->priv->subscriptions), error);

    return TRUE;
}

static void
_eventc_connection_connect_websocket_callback(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    GTask *task = user_data;
    EventcConnection *self = g_task_get_source_object(task);
    GError *error = NULL;
    if ( ( ! eventd_ws_connection_client_connect_finish(_eventc_connection_ws_module, self->priv->ws, res, &error) )
         || ( ! _eventc_connection_connect_after(self, &error) ) )
        g_task_return_error(task, error);
    else
        g_task_return_boolean(task, TRUE);
}

static void
_eventc_connection_connect_callback(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    GTask *task = user_data;
    EventcConnection *self = g_task_get_source_object(task);

    GError *_inner_error_ = NULL;
    GError *error = NULL;
    self->priv->connection = g_socket_client_connect_finish(G_SOCKET_CLIENT(obj), res, &_inner_error_);
    if ( ! _eventc_connection_connect_check(self, _inner_error_, &error) )
        g_task_return_error(task, error);
    else if ( self->priv->ws != NULL )
        eventd_ws_connection_client_connect(_eventc_connection_ws_module, self->priv->ws, G_IO_STREAM(self->priv->connection), _eventc_connection_connect_websocket_callback, task);
    else if ( ! _eventc_connection_connect_after(self, &error) )
        g_task_return_error(task, error);
    else
        g_task_return_boolean(task, TRUE);
}

/**
 * eventc_connection_connect:
 * @connection: an #EventcConnection
 * @callback: (scope async) (closure user_data): a #GAsyncReadyCallback to call when the request is satisfied
 *
 * Initializes the connection to the stored host.
 */
EVENTD_EXPORT
void
eventc_connection_connect(EventcConnection *self, GAsyncReadyCallback callback, gpointer user_data)
{
    g_return_if_fail(EVENTC_IS_CONNECTION(self));

    GTask *task;

    task = g_task_new(self, self->priv->cancellable, callback, user_data);

    GError *error = NULL;

    if ( ! _eventc_connection_expect_disconnected(self, &error) )
    {
        g_task_return_error(task, error);
        return;
    }

    GSocketClient *client;

    client = _eventc_connection_get_socket_client(self);

    g_socket_client_connect_async(client, self->priv->address, self->priv->cancellable, _eventc_connection_connect_callback, task);
    g_object_unref(client);
}

/**
 * eventc_connection_connect_finish:
 * @connection: an #EventcConnection
 * @result: a #GAsyncResult
 * @error: (out) (optional): return location for error or %NULL to ignore
 *
 * Finish an asynchronous operation started with eventc_connection_connect().
 *
 * Returns: %TRUE if the connection completed successfully
 */
EVENTD_EXPORT
gboolean
eventc_connection_connect_finish(EventcConnection *self, GAsyncResult *result, GError **error)
{
    g_return_val_if_fail(EVENTC_IS_CONNECTION(self), FALSE);
    g_return_val_if_fail(g_task_is_valid(result, self), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    g_object_unref(result);
    return g_task_propagate_boolean(G_TASK(result), error);
}

/**
 * eventc_connection_connect_sync:
 * @connection: an #EventcConnection
 * @error: (out) (optional): return location for error or %NULL to ignore
 *
 * Synchronously initializes the connection to the stored host.
 *
 * Returns: %TRUE if the connection completed successfully
 */
EVENTD_EXPORT
gboolean
eventc_connection_connect_sync(EventcConnection *self, GError **error)
{
    g_return_val_if_fail(EVENTC_IS_CONNECTION(self), FALSE);

    if ( ! _eventc_connection_expect_disconnected(self, error) )
        return FALSE;

    GSocketClient *client;

    client = _eventc_connection_get_socket_client(self);

    GError *_inner_error_ = NULL;
    self->priv->connection = g_socket_client_connect(client, self->priv->address, self->priv->cancellable, &_inner_error_);
    g_object_unref(client);

    if ( ! _eventc_connection_connect_check(self, _inner_error_, error) )
        return FALSE;

    if ( self->priv->ws != NULL )
    {
        if ( ! eventd_ws_connection_client_connect_sync(_eventc_connection_ws_module, self->priv->ws, G_IO_STREAM(self->priv->connection), error) )
            return FALSE;
    }

    return _eventc_connection_connect_after(self, error);
}

/**
 * eventc_connection_event:
 * @connection: an #EventcConnection
 * @event: an #EventdEvent to send to the server
 * @error: (out) (optional): return location for error or %NULL to ignore
 *
 * Sends an event across the connection.
 *
 * Returns: %TRUE if the event was sent successfully
 */
EVENTD_EXPORT
gboolean
eventc_connection_event(EventcConnection *self, EventdEvent *event, GError **error)
{
    g_return_val_if_fail(EVENTC_IS_CONNECTION(self), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if ( ! _eventc_connection_expect_connected(self, error) )
        return FALSE;

    return _eventc_connection_send_message(self, eventd_protocol_generate_event(self->priv->protocol, event), error);
}

/**
 * eventc_connection_close:
 * @connection: an #EventcConnection
 * @error: (out) (optional): return location for error or %NULL to ignore
 *
 * Closes the connection. You must wait for the #EventcConnection::disconnected
 * signal before trying to connect again.
 *
 * Returns: %TRUE if the connection was successfully closed
 */
EVENTD_EXPORT
gboolean
eventc_connection_close(EventcConnection *self, GError **error)
{
    g_return_val_if_fail(EVENTC_IS_CONNECTION(self), FALSE);

    GError *_inner_error_ = NULL;
    if ( eventc_connection_is_connected(self, &_inner_error_) )
        _eventc_connection_send_message(self, eventd_protocol_generate_bye(self->priv->protocol, NULL), NULL);
    else if ( _inner_error_ != NULL )
    {
        g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_BYE, "Couldn't send bye message: %s", _inner_error_->message);
        g_error_free(_inner_error_);
        return FALSE;
    }
    else
        return TRUE;

    if ( self->priv->ws != NULL )
        eventd_ws_connection_close(_eventc_connection_ws_module, self->priv->ws);
    else
        g_cancellable_cancel(self->priv->cancellable);

    return TRUE;
}

static void
_eventc_connection_close_internal(EventcConnection *self)
{
    if ( self->priv->error != NULL )
        g_error_free(self->priv->error);
    self->priv->error = NULL;

    if ( self->priv->ws != NULL )
        eventd_ws_connection_cleanup(_eventc_connection_ws_module, self->priv->ws);

    if ( self->priv->out != NULL )
        g_object_unref(self->priv->out);
    if ( self->priv->in != NULL )
        g_object_unref(self->priv->in);

    if ( self->priv->connection != NULL )
    {
        g_io_stream_close(G_IO_STREAM(self->priv->connection), NULL, NULL);
        g_object_unref(self->priv->connection);
    }

    self->priv->out = NULL;
    self->priv->in = NULL;
    self->priv->connection = NULL;

    g_signal_emit(self, _eventc_connection_signals[SIGNAL_DISCONNECTED], 0);
}


/**
 * eventc_connection_set_uri:
 * @connection: an #EventcConnection
 * @uri: (nullable): the URI for the eventd instance to connect to or %NULL for the default local socket
 * @error: (out) (optional): return location for error or %NULL to ignore
 *
 * Sets the URI for the connection.
 * If @uri cannot be resolved, the address of @connection will not change.
 *
 * The format can be:
 * - `unix:@*abstract-name*`
 * - `unix:*path*`
 * - `file://``*path*`
 * - `ws://``*host*[:*port*]/ or `wss://``*host*[:*port*]/`
 *   If you omit `*port*`, the default port will be used.
 * - `evp://``*host*[:*port*]`
 *   If you either omit `*port*` or use `0`, the SRV DNS record will be used.
 *
 * Returns: %TRUE if the host was changed, %FALSE in case of error
 */
EVENTD_EXPORT
gboolean
eventc_connection_set_uri(EventcConnection *self, const gchar *uri, GError **error)
{
    g_return_val_if_fail(EVENTC_IS_CONNECTION(self), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    gchar *default_uri = NULL;
    if ( ( uri == NULL ) || ( *uri == '\0' ) )
        uri = g_getenv("EVENTC_HOST");

    if ( ( uri == NULL ) || ( *uri == '\0' ) )
        uri = default_uri = g_strdup_printf(DEFAULT_SOCKET_SCHEME "%s" G_DIR_SEPARATOR_S PACKAGE_NAME G_DIR_SEPARATOR_S EVP_UNIX_SOCKET, g_get_user_runtime_dir());

    GSocketConnectable *address;
    EventdWsUri *ws_uri = NULL;

    address = _eventc_get_address(uri, &ws_uri, error);
    g_free(default_uri);
    if ( address == NULL )
        return FALSE;
    if ( self->priv->address != NULL )
        g_object_unref(self->priv->address);
    if ( self->priv->ws != NULL )
        eventd_ws_connection_free(_eventc_connection_ws_module, self->priv->ws);
    self->priv->address = address;
    if ( ws_uri != NULL )
    {
        self->priv->ws_tls = eventd_ws_uri_is_tls(_eventc_connection_ws_module, ws_uri);
        self->priv->ws = eventd_ws_connection_client_new(_eventc_connection_ws_module, self, ws_uri, (GDestroyNotify) _eventc_connection_close_internal, self->priv->cancellable, self->priv->protocol);
    }

    return TRUE;
}

/**
 * eventc_connection_set_connectable:
 * @connection: an #EventcConnection
 * @address: (transfer full): a #GSocketConnectable
 *
 * Sets the connectable for the connection.
 */
EVENTD_EXPORT
void
eventc_connection_set_connectable(EventcConnection *self, GSocketConnectable *address)
{
    g_return_if_fail(EVENTC_IS_CONNECTION(self));

    g_object_unref(self->priv->address);
    self->priv->address = address;
}

/**
 * eventc_connection_set_server_identity:
 * @connection: an #EventcConnection
 * @server_identity: the server identity
 *
 * Sets the server identity to check for in the TLS certificate.
 */
EVENTD_EXPORT
void
eventc_connection_set_server_identity(EventcConnection *self, GSocketConnectable *server_identity)
{
    g_return_if_fail(EVENTC_IS_CONNECTION(self));
    g_return_if_fail(G_IS_SOCKET_CONNECTABLE(server_identity));

    if ( self->priv->server_identity != NULL )
        g_object_unref(self->priv->server_identity);
    if ( self->priv->address == server_identity )
        self->priv->server_identity = NULL;
    else
        self->priv->server_identity = g_object_ref(server_identity);
}

/**
 * eventc_connection_set_accept_unknown_ca:
 * @connection: an #EventcConnection
 * @accept_unknown_ca: the accept-insecure-certificate setting
 *
 * Sets whether the TLS connection will accept a certificate signed by
 * an unknown CA.
 */
EVENTD_EXPORT
void
eventc_connection_set_accept_unknown_ca(EventcConnection *self, gboolean accept_unknown_ca)
{
    g_return_if_fail(EVENTC_IS_CONNECTION(self));

    self->priv->accept_unknown_ca = accept_unknown_ca;
}

/**
 * eventc_connection_set_certificate:
 * @connection: an #EventcConnection
 * @certificate: the certificate to use to connect
 *
 * Sets the certificate to use when connecting to a server.
 */
EVENTD_EXPORT
void
eventc_connection_set_certificate(EventcConnection *self, GTlsCertificate *certificate)
{
    g_return_if_fail(EVENTC_IS_CONNECTION(self));
    g_return_if_fail(G_IS_TLS_CERTIFICATE(certificate));

    if ( self->priv->certificate != NULL )
        g_object_unref(self->priv->certificate);
    self->priv->certificate = g_object_ref(certificate);
}


/**
 * eventc_connection_set_subscribe:
 * @connection: an #EventcConnection
 * @subscribe: the subscribe setting
 *
 * Sets whether the connection will subscribe to events.
 * If you do not add specific categories using eventc_connection_add_subscription(),
 * it will subscribe to *all* events.
 */
EVENTD_EXPORT
void
eventc_connection_set_subscribe(EventcConnection *self, gboolean subscribe)
{
    g_return_if_fail(EVENTC_IS_CONNECTION(self));

    self->priv->subscribe = subscribe;
}

/**
 * eventc_connection_add_subscription:
 * @connection: an #EventcConnection
 * @category: (transfer full): a category of events to subscribe to
 *
 * Adds a category of events to subscribe to.
 */
EVENTD_EXPORT
void
eventc_connection_add_subscription(EventcConnection *self, gchar *category)
{
    g_return_if_fail(EVENTC_IS_CONNECTION(self));
    g_return_if_fail(category != NULL);

    if ( self->priv->subscriptions == NULL )
        self->priv->subscriptions = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    g_hash_table_add(self->priv->subscriptions, category);
}

/**
 * eventc_connection_get_subscribe:
 * @connection: an #EventcConnection
 *
 * Retrieves whether the connection will subscribe to events.
 *
 * Returns: %TRUE if the connection will subscribe to events
 */
EVENTD_EXPORT
gboolean
eventc_connection_get_subscribe(EventcConnection *self)
{
    g_return_val_if_fail(EVENTC_IS_CONNECTION(self), FALSE);

    return self->priv->subscribe;
}
