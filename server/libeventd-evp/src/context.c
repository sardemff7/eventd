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
        libeventd_evp_context_close(self);
        return FALSE;
    }

    return ( ( self->connection != NULL ) && g_socket_connection_is_connected(self->connection) );
}

void
libeventd_evp_context_free(LibeventdEvpContext *self)
{
    g_return_if_fail(self != NULL);

    g_object_unref(self->cancellable);

    g_free(self);
}

void
libeventd_evp_context_set_connection(LibeventdEvpContext *self, GSocketConnection *connection)
{
    g_return_if_fail(self != NULL);
    g_return_if_fail(connection != NULL);
    g_return_if_fail(self->connection == NULL);

    g_cancellable_reset(self->cancellable);

    self->connection = g_object_ref(connection);

    self->in  = g_data_input_stream_new(g_io_stream_get_input_stream(G_IO_STREAM(self->connection)));
    self->out = g_data_output_stream_new(g_io_stream_get_output_stream(G_IO_STREAM(self->connection)));

    g_data_input_stream_set_newline_type(self->in, G_DATA_STREAM_NEWLINE_TYPE_LF);
}

void
libeventd_evp_context_close(LibeventdEvpContext *self)
{
    g_return_if_fail(self != NULL);

    g_cancellable_cancel(self->cancellable);

    if ( self->out != NULL )
        g_object_unref(self->out);
    if ( self->in != NULL )
        g_object_unref(self->in);

    if ( self->connection != NULL)
    {
        g_io_stream_close(G_IO_STREAM(self->connection), NULL, NULL);
        g_object_unref(self->connection);
    }

    self->out = NULL;
    self->in = NULL;
    self->connection = NULL;
}
