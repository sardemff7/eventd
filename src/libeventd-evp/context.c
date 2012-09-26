/*
 * libeventd-evp - Library to send EVENT protocol messages
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
#include <gio/gio.h>

#include <libeventd-event.h>

#include <libeventd-evp.h>

#include "context.h"

typedef struct {
    LibeventdEvpContext *context;
    GSimpleAsyncResult *result;
} LibeventdEvpContextCloseData;

GQuark
libeventd_evp_error_quark(void)
{
    return g_quark_from_static_string("libeventd_evp_error-quark");
}

LibeventdEvpContext *
libeventd_evp_context_new(gpointer client, LibeventdEvpClientInterface *interface)
{
    g_return_val_if_fail(client != NULL, NULL);
    g_return_val_if_fail(interface != NULL, NULL);

    LibeventdEvpContext *self;

    self = g_new0(LibeventdEvpContext, 1);
    self->client = client;
    self->interface = interface;

#if GLIB_CHECK_VERSION(2,32,0)
    self->mutex = &self->mutex_;
    g_mutex_init(self->mutex);
#else /* ! GLIB_CHECK_VERSION(2,32,0) */
    self->mutex = g_mutex_new();
#endif /* ! GLIB_CHECK_VERSION(2,32,0) */

    self->cancellable = g_cancellable_new();

    return self;
}

LibeventdEvpContext *
libeventd_evp_context_new_for_connection(gpointer client, LibeventdEvpClientInterface *interface, GSocketConnection *connection)
{
    LibeventdEvpContext *self;

    self = libeventd_evp_context_new(client, interface);
    libeventd_evp_context_set_connection(self, connection);

    return self;
}

gboolean
libeventd_evp_context_is_connected(LibeventdEvpContext *self, GError **error)
{
    g_return_val_if_fail(self != NULL, FALSE);

    if ( self->error != NULL )
    {
        g_propagate_error(error, self->error);
        self->error = NULL;
        return FALSE;
    }

    return ( ( self->connection != NULL ) && g_socket_connection_is_connected(self->connection) );
}

void
libeventd_evp_context_free(LibeventdEvpContext *self)
{
    g_return_if_fail(self != NULL);

    g_object_unref(self->cancellable);

#if GLIB_CHECK_VERSION(2,32,0)
    g_mutex_clear(self->mutex);
#else /* ! GLIB_CHECK_VERSION(2,32,0) */
    g_mutex_free(self->mutex);
#endif /* ! GLIB_CHECK_VERSION(2,32,0) */

    g_free(self);
}

void
libeventd_evp_context_set_connection(LibeventdEvpContext *self, GSocketConnection *connection)
{
    g_return_if_fail(self != NULL);
    g_return_if_fail(connection != NULL);

    self->connection = g_object_ref(connection);

    self->in  = g_data_input_stream_new(g_io_stream_get_input_stream(G_IO_STREAM(self->connection)));
    self->out = g_data_output_stream_new(g_io_stream_get_output_stream(G_IO_STREAM(self->connection)));
}

static gboolean
_libeventd_evp_context_close_complete(gpointer user_data)
{
    LibeventdEvpContextCloseData *data = user_data;
    LibeventdEvpContext *self = data->context;
    GSimpleAsyncResult *result = data->result;

    g_free(data);

    g_object_unref(self->out);
    g_object_unref(self->in);

    g_io_stream_close(G_IO_STREAM(self->connection), NULL, NULL);

    g_object_unref(self->connection);

    self->out = NULL;
    self->in = NULL;
    self->connection = NULL;

    g_simple_async_result_complete_in_idle(result);

    return FALSE;
}

void
libeventd_evp_context_close(LibeventdEvpContext *self, GAsyncReadyCallback callback, gpointer user_data)
{
    g_return_if_fail(self != NULL);

    g_cancellable_cancel(self->cancellable);

    LibeventdEvpContextCloseData *data;

    data = g_new0(LibeventdEvpContextCloseData, 1);
    data->context = self;
    data->result = g_simple_async_result_new(G_OBJECT(self->cancellable), callback, user_data, libeventd_evp_context_close);

    g_idle_add(_libeventd_evp_context_close_complete, data);
}

void
libeventd_evp_context_close_finish(LibeventdEvpContext *self, GAsyncResult *result)
{
    g_return_if_fail(self != NULL);
    g_return_if_fail(g_simple_async_result_is_valid(result, G_OBJECT(self->cancellable), libeventd_evp_context_close));
}

gboolean
libeventd_evp_context_lock(LibeventdEvpContext *self, GSourceFunc callback, gpointer user_data)
{
    g_return_val_if_fail(self != NULL, FALSE);

    if ( g_mutex_trylock(self->mutex) )
        return TRUE;
    g_idle_add(callback, user_data);
    return FALSE;
}

void
libeventd_evp_context_unlock(LibeventdEvpContext *self)
{
    g_return_if_fail(self != NULL);

    g_mutex_unlock(self->mutex);
}
