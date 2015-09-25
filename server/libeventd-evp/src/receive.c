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

#include <config.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <glib.h>
#include <gio/gio.h>

#include <libeventd-event.h>

#include <libeventd-evp.h>

#include "context.h"

gboolean
libeventd_evp_context_passive(LibeventdEvpContext *self, GError **error)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(self->in != NULL, FALSE);

    g_cancellable_reset(self->cancellable);

    if ( ! libeventd_evp_context_send_passive(self, error) )
        return FALSE;

    g_object_unref(self->in);
    self->in = NULL;

    return TRUE;
}

static void
_libeventd_evp_new_receive_finish(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    g_return_if_fail(G_IS_ASYNC_RESULT(res));
    LibeventdEvpContext *self = user_data;
    gchar *line;

    line = g_data_input_stream_read_line_finish(G_DATA_INPUT_STREAM(obj), res, NULL, &self->error);
    if ( line == NULL )
    {
        if ( ( self->error != NULL ) && ( self->error->code == G_IO_ERROR_CANCELLED ) )
            g_clear_error(&self->error);
        return;
    }

    if ( ! eventd_protocol_parse(self->protocol, &line, &self->error) )
        return;

    g_data_input_stream_read_line_async(self->in, self->priority, self->cancellable, _libeventd_evp_new_receive_finish, self);
}

void
libeventd_evp_context_receive_loop(LibeventdEvpContext *self, gint priority)
{
    g_return_if_fail(self != NULL);

    self->priority = priority;

    g_cancellable_reset(self->cancellable);

    g_data_input_stream_read_line_async(self->in, self->priority, self->cancellable, _libeventd_evp_new_receive_finish, self);
}
