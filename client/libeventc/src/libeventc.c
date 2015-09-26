/*
 * libeventc - Library to communicate with eventd
 *
 * Copyright © 2011-2012 Quentin "Sardem FF7" Glidic
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

#include <config.h>

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#ifdef HAVE_GIO_UNIX
#include <gio/gunixsocketaddress.h>
#endif /* HAVE_GIO_UNIX */

#include <libeventd-event.h>
#include <libeventd-protocol.h>

#include <libeventc.h>
#define EVENTC_CONNECTION_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), EVENTC_TYPE_CONNECTION, EventcConnectionPrivate))

EVENTD_EXPORT GType eventc_connection_get_type(void);
G_DEFINE_TYPE(EventcConnection, eventc_connection, G_TYPE_OBJECT)

struct _EventcConnectionPrivate {
    GSocketConnectable *address;
    gboolean passive;
    gboolean enable_proxy;
    GError *error;
    EventdProtocol* protocol;
    GCancellable *cancellable;
    GSocketConnection *connection;
    GDataInputStream *in;
    GDataOutputStream *out;
    GHashTable* events;
};

typedef struct {
    EventdEvent *event;
    gulong answered;
    gulong ended;
} EventdConnectionEventHandlers;

typedef struct {
    EventcConnection *self;
    GAsyncReadyCallback callback;
    gpointer user_data;
} EventcConnectionCallbackData;

static void _eventc_connection_close_internal(EventcConnection *self);

static GSocketConnectable *
_eventc_get_address(const gchar *host_and_port, GError **error)
{
    GSocketConnectable *address = NULL;

    if ( ( host_and_port == NULL ) || ( *host_and_port == '\0' ) )
        host_and_port = "localhost";

#ifdef HAVE_GIO_UNIX
    gchar *path = NULL;
    if ( g_strcmp0(host_and_port, "localhost") == 0 )
        path = g_build_filename(g_get_user_runtime_dir(), PACKAGE_NAME, EVP_UNIX_SOCKET, NULL);
    else if ( g_path_is_absolute(host_and_port) )
        path = g_strdup(host_and_port);
    if ( ( path != NULL ) && g_file_test(path, G_FILE_TEST_EXISTS) && ( ! g_file_test(path, G_FILE_TEST_IS_DIR|G_FILE_TEST_IS_REGULAR) ) )
        address = G_SOCKET_CONNECTABLE(g_unix_socket_address_new(path));
    g_free(path);
#endif /* HAVE_GIO_UNIX */

    if ( address != NULL )
        return address;

    GError *_inner_error_ = NULL;

    address = g_network_address_parse(host_and_port, 0, &_inner_error_);

    if ( address == NULL )
    {
        g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_HOSTNAME, "Could not resolve host name '%s': %s", host_and_port, _inner_error_->message);
        g_error_free(_inner_error_);
    }
    else if ( g_network_address_get_port(G_NETWORK_ADDRESS(address)) == 0 )
    {
        g_object_unref(address);
        address = g_network_service_new(EVP_SERVICE_NAME, EVP_TRANSPORT_NAME, host_and_port);
    }

    return address;
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
    return PACKAGE_VERSION;
}

EVENTD_EXPORT
GQuark
eventc_error_quark(void)
{
    return g_quark_from_static_string("eventc_error-quark");
}

static gboolean
_eventc_connection_send_message(EventcConnection *self, gchar *message)
{
    gboolean r = FALSE;
    if ( self->priv->error != NULL )
        goto end;

    GError *error = NULL;

#ifdef EVENTD_DEBUG
    g_debug("Sending message:\n%s", message);
#endif /* EVENTD_DEBUG */

    if ( g_data_output_stream_put_string(self->priv->out, message, NULL, &error) )
        r = TRUE;
    else
    {
        g_set_error(&self->priv->error, EVENTC_ERROR, EVENTC_ERROR_CONNECTION, "Failed to send message: %s", error->message);
        g_error_free(error);
        g_cancellable_cancel(self->priv->cancellable);
    }

end:
    g_free(message);
    return r;
}
static void
_eventc_connection_event_answered(EventcConnection *self, const gchar *answer, EventdEvent *event)
{
    EventdConnectionEventHandlers *handlers;
    handlers = g_hash_table_lookup(self->priv->events, event);
    g_return_if_fail(handlers != NULL);

    g_signal_handler_disconnect(event, handlers->answered);
    handlers->answered = 0;
    _eventc_connection_send_message(self, eventd_protocol_generate_answered(self->priv->protocol, event, answer));
}

static void
_eventc_connection_protocol_answered(EventcConnection *self, EventdEvent *event, const gchar *answer, EventdProtocol *protocol)
{
    EventdConnectionEventHandlers *handlers;
    handlers = g_hash_table_lookup(self->priv->events, event);
    g_return_if_fail(handlers != NULL);

    g_signal_handler_disconnect(event, handlers->answered);
    handlers->answered = 0;
    eventd_event_answer(event, answer);
}

static void
_eventc_connection_event_ended(EventcConnection *self, EventdEventEndReason reason, EventdEvent *event)
{
    g_hash_table_remove(self->priv->events, event);
    _eventc_connection_send_message(self, eventd_protocol_generate_ended(self->priv->protocol, event, reason));
}

static void
_eventc_connection_protocol_ended(EventcConnection *self, EventdEvent *event, EventdEventEndReason reason, EventdProtocol *protocol)
{
    g_hash_table_remove(self->priv->events, event);
    eventd_event_end(event, reason);
}

static void
_eventc_connection_protocol_bye(EventcConnection *self, EventdProtocol *protocol)
{
    _eventc_connection_close_internal(self);
}

static void
_eventc_connection_event_handlers_free(gpointer data)
{
    EventdConnectionEventHandlers *handlers = data;

    if ( handlers->answered > 0 )
        g_signal_handler_disconnect(handlers->event, handlers->answered);
    g_signal_handler_disconnect(handlers->event, handlers->ended);

    g_object_unref(handlers->event);

    g_slice_free(EventdConnectionEventHandlers, handlers);
}

static void
_eventc_connection_read_callback(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    EventcConnection *self = user_data;
    GError *error = NULL;
    gchar *line;

    line = g_data_input_stream_read_line_finish(G_DATA_INPUT_STREAM(obj), res, NULL, &error);
    if ( line == NULL )
    {
        if ( error != NULL )
        {
            if ( error->code != G_IO_ERROR_CANCELLED )
                g_set_error(&self->priv->error, EVENTC_ERROR, EVENTC_ERROR_CONNECTION, "Could not read line: %s", error->message);
            g_clear_error(&error);
        }
        return;
    }

    if ( ! eventd_protocol_parse(self->priv->protocol, &line, &self->priv->error) )
        return;

    g_data_input_stream_read_line_async(self->priv->in, G_PRIORITY_DEFAULT, self->priv->cancellable, _eventc_connection_read_callback, self);
}

static void _eventc_connection_finalize(GObject *object);

static void
eventc_connection_class_init(EventcConnectionClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(EventcConnectionPrivate));

    eventc_connection_parent_class = g_type_class_peek_parent(klass);

    object_class->finalize = _eventc_connection_finalize;
}

static void
eventc_connection_init(EventcConnection *self)
{
    self->priv = EVENTC_CONNECTION_GET_PRIVATE(self);

    self->priv->protocol = eventd_protocol_evp_new();
    self->priv->events = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, _eventc_connection_event_handlers_free);
    self->priv->cancellable = g_cancellable_new();

    g_signal_connect_swapped(self->priv->protocol, "answered", G_CALLBACK(_eventc_connection_protocol_answered), self);
    g_signal_connect_swapped(self->priv->protocol, "ended", G_CALLBACK(_eventc_connection_protocol_ended), self);
    g_signal_connect_swapped(self->priv->protocol, "bye", G_CALLBACK(_eventc_connection_protocol_bye), self);
}

static void
_eventc_connection_finalize(GObject *object)
{
    EventcConnection *self = EVENTC_CONNECTION(object);

    if ( self->priv->address != NULL )
        g_object_unref(self->priv->address);

    g_object_unref(self->priv->cancellable);
    g_hash_table_unref(self->priv->events);
    g_object_unref(self->priv->protocol);

    G_OBJECT_CLASS(eventc_connection_parent_class)->finalize(object);
}

/**
 * eventc_connection_new:
 * @host: the host running the eventd instance to connect to
 * @error: (out) (optional): return location for error or %NULL to ignore
 *
 * Creates a new connection to an eventd daemon.
 * See eventc_connection_set_host() for the exact format for @host.
 *
 * Returns: (transfer full): a new connection, or %NULL if @host could not be resolved
 */
EVENTD_EXPORT
EventcConnection *
eventc_connection_new(const gchar *host, GError **error)
{
    g_return_val_if_fail(host != NULL, NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    EventcConnection *self;

    self = g_object_new(EVENTC_TYPE_CONNECTION, NULL);

    if ( ! eventc_connection_set_host(self, host, error) )
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

static gboolean
_eventc_connection_connect_after(EventcConnection *self, GError *_inner_error_, GError **error)
{
    if ( self->priv->connection == NULL )
    {
        g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_CONNECTION, "Failed to connect: %s", _inner_error_->message);
        g_error_free(_inner_error_);
        return FALSE;
    }

    self->priv->out = g_data_output_stream_new(g_io_stream_get_output_stream(G_IO_STREAM(self->priv->connection)));

    if ( self->priv->passive )
        return _eventc_connection_send_message(self, eventd_protocol_generate_passive(self->priv->protocol));

    self->priv->in = g_data_input_stream_new(g_io_stream_get_input_stream(G_IO_STREAM(self->priv->connection)));

    g_cancellable_reset(self->priv->cancellable);
    g_data_input_stream_read_line_async(self->priv->in, G_PRIORITY_DEFAULT, self->priv->cancellable, _eventc_connection_read_callback, self);

    return TRUE;
}

static void
_eventc_connection_connect_callback(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    EventcConnectionCallbackData *data = user_data;
    EventcConnection *self = data->self;
    GAsyncReadyCallback callback = data->callback;
    user_data = data->user_data;
    g_slice_free(EventcConnectionCallbackData, data);

    GError *_inner_error_ = NULL;
    GError *error = NULL;
    self->priv->connection = g_socket_client_connect_finish(G_SOCKET_CLIENT(obj), res, &_inner_error_);
    if ( ! _eventc_connection_connect_after(self, _inner_error_, &error) )
    {
        g_simple_async_report_take_gerror_in_idle(G_OBJECT(self), callback, user_data, error);
        return;
    }

    GSimpleAsyncResult *result;
    result = g_simple_async_result_new(G_OBJECT(self), callback, user_data, _eventc_connection_connect_callback);
    g_simple_async_result_complete_in_idle(result);
    g_object_unref(result);
}

/**
 * eventc_connection_connect:
 * @connection: an #EventcConnection
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Initializes the connection to the stored host.
 */
EVENTD_EXPORT
void
eventc_connection_connect(EventcConnection *self, GAsyncReadyCallback callback, gpointer user_data)
{
    g_return_if_fail(EVENTC_IS_CONNECTION(self));

    GError *error = NULL;

    if ( ! _eventc_connection_expect_disconnected(self, &error) )
    {
        g_simple_async_report_take_gerror_in_idle(G_OBJECT(self), callback, user_data, error);
        return;
    }

    GSocketClient *client;

    client = g_socket_client_new();
    g_socket_client_set_enable_proxy(client, self->priv->enable_proxy);

    EventcConnectionCallbackData *data;
    data = g_slice_new(EventcConnectionCallbackData);
    data->self = self;
    data->callback = callback;
    data->user_data = user_data;

    g_socket_client_connect_async(client, self->priv->address, NULL, _eventc_connection_connect_callback, data);
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
    g_return_val_if_fail(g_simple_async_result_is_valid(result, G_OBJECT(self), NULL), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT(result);

    if ( g_simple_async_result_propagate_error(simple, error) )
        return FALSE;

    return TRUE;
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

    client = g_socket_client_new();
    g_socket_client_set_enable_proxy(client, self->priv->enable_proxy);

    GError *_inner_error_ = NULL;
    self->priv->connection = g_socket_client_connect(client, self->priv->address, NULL, &_inner_error_);
    g_object_unref(client);

    return _eventc_connection_connect_after(self, _inner_error_, error);
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
    g_return_val_if_fail(EVENTD_IS_EVENT(event), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    EventdEvent *old;
    old = g_hash_table_lookup(self->priv->events, event);
    if ( old != NULL )
    {
        g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_EVENT, "This event was already sent to the server");
        return FALSE;
    }

    if ( ! _eventc_connection_expect_connected(self, error) )
        return FALSE;

    if ( ! _eventc_connection_send_message(self, eventd_protocol_generate_event(self->priv->protocol, event)) )
    {
        g_propagate_error(error, self->priv->error);
        self->priv->error = NULL;
        return FALSE;
    }

    EventdConnectionEventHandlers *handlers;
    handlers = g_slice_new(EventdConnectionEventHandlers);
    handlers->event = g_object_ref(event);

    handlers->answered = g_signal_connect_swapped(event, "answered", G_CALLBACK(_eventc_connection_event_answered), self);
    handlers->ended = g_signal_connect_swapped(event, "ended", G_CALLBACK(_eventc_connection_event_ended), self);

    g_hash_table_insert(self->priv->events, event, handlers);

    return TRUE;
}

/**
 * eventc_connection_event_close:
 * @connection: an #EventcConnection
 * @error: (out) (optional): return location for error or %NULL to ignore
 *
 * Closes the connection.
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
        _eventc_connection_send_message(self, eventd_protocol_generate_bye(self->priv->protocol, NULL));
    else if ( _inner_error_ != NULL )
    {
        g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_BYE, "Couldn't send bye message: %s", _inner_error_->message);
        g_error_free(_inner_error_);
        return FALSE;
    }

    _eventc_connection_close_internal(self);

    return TRUE;
}

static void
_eventc_connection_close_internal(EventcConnection *self)
{
    g_cancellable_cancel(self->priv->cancellable);

    if ( self->priv->error != NULL )
        g_error_free(self->priv->error);
    self->priv->error = NULL;

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

    g_hash_table_remove_all(self->priv->events);
}


/**
 * eventc_connection_set_host:
 * @connection: an #EventcConnection
 * @host: the host running the eventd instance to connect to
 * @error: (out) (optional): return location for error or %NULL to ignore
 *
 * Sets the host for the connection.
 * If @host cannot be resolved, the address of @connection will not change.
 *
 * The format is `*host*[:*port*]`.
 * If you either omit `*port*` or use `0`, the SRV DNS record will be used.
 *
 * Returns: %TRUE if the host was changed, %FALSE in case of error
 */
EVENTD_EXPORT
gboolean
eventc_connection_set_host(EventcConnection *self, const gchar *host, GError **error)
{
    g_return_val_if_fail(EVENTC_IS_CONNECTION(self), FALSE);
    g_return_val_if_fail(host != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    GSocketConnectable *address;

    address = _eventc_get_address(host, error);
    if ( address == NULL )
        return FALSE;
    self->priv->address = address;

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
 * eventc_connection_set_passive:
 * @connection: an #EventcConnection
 * @passive: the passive setting
 *
 * Sets whether the connection is passive or not. A passive connection does not
 * receive events back from the server so that the client does not require an
 * event loop.
 */
EVENTD_EXPORT
void
eventc_connection_set_passive(EventcConnection *self, gboolean passive)
{
    g_return_if_fail(EVENTC_IS_CONNECTION(self));

    self->priv->passive = passive;
}

/**
 * eventc_connection_set_enable_proxy:
 * @connection: an #EventcConnection
 * @enable_proxy: the passive setting
 *
 * Sets whether the connection is to use a proxy or not on the underlying
 * #GSocketClient.
 */
EVENTD_EXPORT
void
eventc_connection_set_enable_proxy(EventcConnection *self, gboolean enable_proxy)
{
    g_return_if_fail(EVENTC_IS_CONNECTION(self));

    self->priv->enable_proxy = enable_proxy;
}

/**
 * eventc_connection_get_passive:
 * @connection: an #EventcConnection
 *
 * Retrieves whether the connection is passive or not.
 *
 * Returns: %TRUE if the connection is passive
 */
EVENTD_EXPORT
gboolean
eventc_connection_get_passive(EventcConnection *self)
{
    g_return_val_if_fail(EVENTC_IS_CONNECTION(self), FALSE);

    return self->priv->passive;
}

/**
 * eventc_connection_get_enable_proxy:
 * @connection: an #EventcConnection
 *
 * Retrieves whether the connection is to use a proxy or not.
 *
 * Returns: %TRUE if the connection is using a proxy
 */
EVENTD_EXPORT
gboolean
eventc_connection_get_enable_proxy(EventcConnection *self)
{
    g_return_val_if_fail(EVENTC_IS_CONNECTION(self), FALSE);

    return self->priv->enable_proxy;
}
