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


static gboolean _libeventd_evp_context_send_message(LibeventdEvpContext *self, gchar *message, GError **error);

gboolean
libeventd_evp_context_send_passive(LibeventdEvpContext *self, GError **error)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    gchar *message;

    message = eventd_protocol_generate_passive(self->protocol);
    return _libeventd_evp_context_send_message(self, message, error);
}

gboolean
libeventd_evp_context_send_event(LibeventdEvpContext *self, EventdEvent *event, GError **error)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if ( self->out == NULL )
        /* Passive mode */
        return TRUE;

    gchar *message;

    message = eventd_protocol_generate_event(self->protocol, event);
    return _libeventd_evp_context_send_message(self, message, error);
}

gboolean
libeventd_evp_context_send_answered(LibeventdEvpContext *self, EventdEvent *event, const gchar *answer, GError **error)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if ( self->out == NULL )
        /* Passive mode */
        return TRUE;

    gchar *message;

    message = eventd_protocol_generate_answered(self->protocol, event, answer);
    return _libeventd_evp_context_send_message(self, message, error);
}

gboolean
libeventd_evp_context_send_ended(LibeventdEvpContext *self, EventdEvent *event, EventdEventEndReason reason, GError **error)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if ( self->out == NULL )
        /* Passive mode */
        return TRUE;

    gchar *message;

    message = eventd_protocol_generate_ended(self->protocol, event, reason);
    return _libeventd_evp_context_send_message(self, message, error);
}


void
libeventd_evp_context_send_bye(LibeventdEvpContext *self)
{
    g_return_if_fail(self != NULL);

    if ( self->out == NULL )
        /* Passive mode */
        return;

    gchar *message;

    message = eventd_protocol_generate_bye(self->protocol, NULL);
    _libeventd_evp_context_send_message(self, message, NULL);
}

static gboolean
_libeventd_evp_context_send_message(LibeventdEvpContext *self, gchar *message, GError **error)
{
    if ( message == NULL )
        return FALSE;

    GError *_inner_error_ = NULL;

#ifdef EVENTD_DEBUG
    g_debug("Sending line: %s", message);
#endif /* EVENTD_DEBUG */

    if ( ! g_data_output_stream_put_string(self->out, message, self->cancellable, &_inner_error_) )
    {
        g_propagate_error(error, _inner_error_);
        g_free(message);
        libeventd_evp_context_close(self);
        return FALSE;
    }

    g_free(message);
    return TRUE;
}
