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

#include <libeventd-evp.h>
#include <libeventd-event.h>

#include <libeventc.h>
#define EVENTC_CONNECTION_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), EVENTC_TYPE_CONNECTION, EventcConnectionPrivate))

EVENTD_EXPORT GType eventc_connection_get_type(void);
G_DEFINE_TYPE(EventcConnection, eventc_connection, G_TYPE_OBJECT)

struct _EventcConnectionPrivate {
    gchar* host;
    gboolean passive;
    gboolean enable_proxy;
    LibeventdEvpContext* evp;
    guint64 count;
    GHashTable* events;
    GHashTable* ids;
};

typedef struct {
    EventcConnection *self;
    GAsyncReadyCallback callback;
    gpointer user_data;
} EventcConnectionCallbackData;

static void _eventc_connection_close_internal(EventcConnection *self);

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

static void
_eventc_connection_error(gpointer data, LibeventdEvpContext *context, GError *error)
{
    g_warning("Error: %s", error->message);
}

static void
_eventc_connection_answered(gpointer data, LibeventdEvpContext *context, const gchar *id, const gchar *answer, GHashTable *data_hash)
{
    EventcConnection *self = data;
    EventdEvent *event;
    event = g_hash_table_lookup(self->priv->events, id);
    if ( event == NULL )
        return;

    eventd_event_set_all_answer_data(event, data_hash);
    eventd_event_answer(event, answer);
}

static void
_eventc_connection_ended(gpointer data, LibeventdEvpContext *context, const gchar *id, EventdEventEndReason reason)
{
    EventcConnection *self = data;
    EventdEvent *event;
    event = g_hash_table_lookup(self->priv->events, id);
    if ( event == NULL )
        return;

    eventd_event_end(event, reason);
}

static void
_eventc_connection_bye(gpointer data, LibeventdEvpContext *context)
{
    EventcConnection *self = data;
    _eventc_connection_close_internal(self);
}

static LibeventdEvpClientInterface _eventc_connection_client_interface = {
    .error = _eventc_connection_error,

    .event = NULL,
    .end = NULL,

    .answered = _eventc_connection_answered,
    .ended = _eventc_connection_ended,

    .bye = _eventc_connection_bye
};

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
}

static void
_eventc_connection_finalize(GObject *object)
{
    EventcConnection *self = EVENTC_CONNECTION(object);
    EventcConnectionPrivate *priv = self->priv;

    g_hash_table_unref(self->priv->events);
    g_hash_table_unref(self->priv->ids);
    g_free(priv->host);

    G_OBJECT_CLASS(eventc_connection_parent_class)->finalize(object);
}

/**
 * eventc_connection_new:
 * @host: the host running the eventd instance to connect to
 *
 * Creates a new connection to an eventd daemon.
 *
 * Returns: (transfer full): a new connection
 */
EVENTD_EXPORT
EventcConnection *
eventc_connection_new(const gchar *host)
{
    EventcConnection *self;

    self = g_object_new(EVENTC_TYPE_CONNECTION, NULL);

    self->priv->host = g_strdup(host);

    self->priv->evp = libeventd_evp_context_new(self, &_eventc_connection_client_interface);
    self->priv->events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);
    self->priv->ids = g_hash_table_new_full(g_direct_hash, g_direct_equal, g_object_unref, NULL);

    return self;
}

/**
 * eventc_connection_is_connected:
 * @self: an #EventcConnection
 * @error: (out) (optional): a #GError or %NULL
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
    GError *_inner_error_ = NULL;
    if ( libeventd_evp_context_is_connected(self->priv->evp, &_inner_error_) )
        return TRUE;

    if ( _inner_error_ != NULL )
    {
        g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_CONNECTION, "Connection error: %s", _inner_error_->message);
        g_error_free(_inner_error_);
    }

    return FALSE;
}

static gboolean
_eventc_connection_connect_before(EventcConnection *self, GSocketClient **client, GSocketConnectable **address, GError **error)
{
    GError *_inner_error_ = NULL;

    if ( libeventd_evp_context_is_connected(self->priv->evp, &_inner_error_) )
    {
        g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_ALREADY_CONNECTED, "Already connected, you must disconnect first");
        return FALSE;
    }
    if ( _inner_error_ != NULL )
    {
        g_propagate_error(error, _inner_error_);
        return FALSE;
    }

    *address = libeventd_evp_get_address(self->priv->host, &_inner_error_);
    if ( *address == NULL )
    {
        g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_HOSTNAME, "Couldn't resolve the hostname '%s': %s", self->priv->host, _inner_error_->message);
        g_error_free(_inner_error_);
        return FALSE;
    }

    *client = g_socket_client_new();
    g_socket_client_set_enable_proxy(*client, self->priv->enable_proxy);

    return TRUE;
}

static gboolean
_eventc_connection_connect_after(EventcConnection *self, GSocketConnection *connection, GError *_inner_error_, GError **error)
{
    if ( connection == NULL )
    {
        g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_CONNECTION, "Failed to connect: %s", _inner_error_->message);
        g_error_free(_inner_error_);
        return FALSE;
    }

    libeventd_evp_context_set_connection(self->priv->evp, connection);
    if ( self->priv->passive )
    {
        if ( ! libeventd_evp_context_passive(self->priv->evp, &_inner_error_) )
        {
            g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_CONNECTION, "Failed to go into passive mode: %s", _inner_error_->message);
            g_error_free(_inner_error_);
            return FALSE;
        }
    }
    else
        libeventd_evp_context_receive_loop(self->priv->evp, G_PRIORITY_DEFAULT);

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
    GSocketConnection *connection;
    connection = g_socket_client_connect_finish(G_SOCKET_CLIENT(obj), res, &_inner_error_);
    if ( ! _eventc_connection_connect_after(self, connection, _inner_error_, &error) )
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
 * @self: an #EventcConnection
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

    GSocketClient *client;
    GSocketConnectable *address;
    if ( ! _eventc_connection_connect_before(self, &client, &address, &error) )
    {
        g_simple_async_report_take_gerror_in_idle(G_OBJECT(self), callback, user_data, error);
        return;
    }

    EventcConnectionCallbackData *data;
    data = g_slice_new(EventcConnectionCallbackData);
    data->self = self;
    data->callback = callback;
    data->user_data = user_data;

    g_socket_client_connect_async(client, address, NULL, _eventc_connection_connect_callback, data);
    g_object_unref(address);
    g_object_unref(client);
}

/**
 * eventc_connection_connect_finish:
 * @self: an #EventcConnection
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
 * @self: an #EventcConnection
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

    GSocketClient *client;
    GSocketConnectable *address;
    if ( ! _eventc_connection_connect_before(self, &client, &address, error) )
        return FALSE;

    GError *_inner_error_ = NULL;
    GSocketConnection *connection;
    connection = g_socket_client_connect(client, address, NULL, &_inner_error_);
    g_object_unref(address);
    g_object_unref(client);

    if ( ! _eventc_connection_connect_after(self, connection, _inner_error_, error) )
        return FALSE;

    return TRUE;
}

static void
_eventc_connection_event_end_callback(EventcConnection *self, EventdEventEndReason reason, EventdEvent *event)
{
    const gchar *id;
    id = g_hash_table_lookup(self->priv->ids, event);
    g_return_if_fail(id != NULL);

    g_hash_table_remove(self->priv->events, id);
    g_hash_table_remove(self->priv->ids, event);

    g_signal_handlers_disconnect_by_func(G_OBJECT(event), _eventc_connection_event_end_callback, self);
}

/**
 * eventc_connection_event:
 * @self: an #EventcConnection
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
    old = g_hash_table_lookup(self->priv->ids, event);
    if ( old != NULL )
    {
        g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_EVENT, "This event was already sent to the server");
        return FALSE;
    }

    GError *_inner_error_ = NULL;
    if ( ! libeventd_evp_context_is_connected(self->priv->evp, &_inner_error_) )
    {
        if ( _inner_error_ != NULL )
        {
            g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_CONNECTION, "Connection error: %s", _inner_error_->message);
            g_error_free(_inner_error_);
        }
        else
            g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_NOT_CONNECTED, "Not connected, you must connect first");
        return FALSE;
    }

    gchar *id;

    id = g_strdup_printf("%" G_GINT64_MODIFIER "x", ++self->priv->count);
    if ( ! libeventd_evp_context_send_event(self->priv->evp, id, event, &_inner_error_) )
    {
        g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_EVENT, "Couldn't send event: %s", _inner_error_->message);
        g_error_free(_inner_error_);
        return FALSE;
    }

    if ( ! self->priv->passive )
    {
        g_hash_table_insert(self->priv->events, id, g_object_ref(event));
        g_hash_table_insert(self->priv->ids, g_object_ref(event), id);
        g_signal_connect_swapped(event, "ended", G_CALLBACK(_eventc_connection_event_end_callback), self);
    }

    return TRUE;
}

/**
 * eventc_connection_event_end:
 * @self: an #EventcConnection
 * @event: an #EventdEvent to send to the server
 * @error: (out) (optional): return location for error or %NULL to ignore
 *
 * End the event (e.g., close the notification popup).
 *
 * Returns: %TRUE if the event was ended successfully
 */
EVENTD_EXPORT
gboolean
eventc_connection_event_end(EventcConnection *self, EventdEvent *event, GError **error)
{
    g_return_val_if_fail(EVENTC_IS_CONNECTION(self), FALSE);
    g_return_val_if_fail(EVENTD_IS_EVENT(event), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    GError *_inner_error_ = NULL;
    if ( ! libeventd_evp_context_is_connected(self->priv->evp, &_inner_error_) )
    {
        if ( _inner_error_ != NULL )
        {
            g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_CONNECTION, "Connection error: %s", _inner_error_->message);
            g_error_free(_inner_error_);
        }
        else
            g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_NOT_CONNECTED, "Not connected, you must connect first");
        return FALSE;
    }

    const gchar *id;
    id = g_hash_table_lookup(self->priv->ids, event);
    if ( id == NULL )
    {
        g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_EVENT, "Couldn't find event");
        return FALSE;
    }
    if ( ! libeventd_evp_context_send_end(self->priv->evp, id, &_inner_error_) )
    {
        g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_EVENT, "Couldn't send event end: %s", _inner_error_->message);
        g_error_free(_inner_error_);
        return FALSE;
    }

    return TRUE;
}

/**
 * eventc_connection_event_close:
 * @self: an #EventcConnection
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
    if ( libeventd_evp_context_is_connected(self->priv->evp, &_inner_error_) )
        libeventd_evp_context_send_bye(self->priv->evp);
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
    libeventd_evp_context_close(self->priv->evp);

    GHashTableIter iter;
    EventdEvent *event;
    g_hash_table_iter_init(&iter, self->priv->events);
    while ( g_hash_table_iter_next(&iter, NULL, (gpointer *)&event) )
        g_signal_handlers_disconnect_by_func(G_OBJECT(event), _eventc_connection_event_end_callback, self);

    g_hash_table_remove_all(self->priv->events);
    g_hash_table_remove_all(self->priv->ids);
}


/**
 * eventc_connection_set_host:
 * @self: an #EventcConnection
 * @value: the host running the eventd instance to connect to
 *
 * Sets the host for the connection.
 */
EVENTD_EXPORT
void
eventc_connection_set_host(EventcConnection *self, const gchar *host)
{
    g_return_if_fail(EVENTC_IS_CONNECTION(self));
    g_return_if_fail(host != NULL);

    g_free(self->priv->host);
    self->priv->host = g_strdup(host);
}

/**
 * eventc_connection_set_passive:
 * @self: an #EventcConnection
 * @value: the passive setting
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
 * @self: an #EventcConnection
 * @value: the passive setting
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
 * @self: an #EventcConnection
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
 * @self: an #EventcConnection
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
