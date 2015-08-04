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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <glib.h>
#include <gio/gio.h>

#include <libeventd-event.h>

#include <libeventd-evp.h>

#include "context.h"

static gboolean
_libeventd_evp_context_send_data(LibeventdEvpContext *self, GHashTable *all_data, GError **error)
{
    if ( all_data == NULL )
        return TRUE;

    gchar *message;

    GHashTableIter iter;
    const gchar *name;
    const gchar *content;

    g_hash_table_iter_init(&iter, all_data);
    while ( g_hash_table_iter_next(&iter, (gpointer *)&name, (gpointer *)&content) )
    {
        if ( g_utf8_strchr(content, -1, '\n') == NULL )
        {
            message = g_strdup_printf("DATA %s %s", name, content);
            if ( ! libeventd_evp_context_send_message(self, message, error) )
                goto fail;
            g_free(message);
        }
        else
        {
            message = g_strdup_printf(".DATA %s", name);
            if ( ! libeventd_evp_context_send_message(self, message, error) )
                goto fail;
            g_free(message);

            gchar **data;
            data = g_strsplit(content, "\n", -1);

            gchar **line;
            for ( line = data ; *line != NULL ; ++line )
            {
                if ( (*line)[0] == '.' )
                {
                    GError *_inner_error_ = NULL;
                    if ( ! g_data_output_stream_put_byte(self->out, '.', self->cancellable, &_inner_error_) )
                    {
                        g_propagate_error(error, _inner_error_);
                        g_strfreev(data);
                        return FALSE;
                    }
                }
                if ( ! libeventd_evp_context_send_message(self, *line, error) )
                {
                    g_strfreev(data);
                    return FALSE;
                }
            }

            g_strfreev(data);
            if ( ! libeventd_evp_context_send_message(self, ".", error) )
                return FALSE;
        }
    }

    return TRUE;

fail:
    g_free(message);
    return FALSE;
}

gboolean
libeventd_evp_context_send_event(LibeventdEvpContext *self, const gchar *id, EventdEvent *event, GError **error)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(id != NULL, FALSE);
    g_return_val_if_fail(event != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if ( self->out == NULL )
        /* Passive mode */
        return TRUE;

    gchar *message;

    message = g_strdup_printf(".EVENT %s %s %s", id, eventd_event_get_category(event), eventd_event_get_name(event));
    if ( ! libeventd_evp_context_send_message(self, message, error) )
        goto fail;
    g_free(message);

    GList *answer_;
    for ( answer_ = eventd_event_get_answers(event) ; answer_ != NULL ; answer_ = g_list_next(answer_) )
    {
        const gchar *answer = answer_->data;
        message = g_strdup_printf("ANSWER %s", answer);
        if ( ! libeventd_evp_context_send_message(self, message, error) )
            goto fail;
        g_free(message);
    }

    if ( ! _libeventd_evp_context_send_data(self, eventd_event_get_all_data(event), error) )
        return FALSE;

    if ( ! libeventd_evp_context_send_message(self, ".", error) )
        return FALSE;

    return TRUE;

fail:
    g_free(message);
    return FALSE;
}

gboolean
libeventd_evp_context_send_end(LibeventdEvpContext *self, const gchar *id, GError **error)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(id != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if ( self->out == NULL )
        /* Passive mode */
        return TRUE;

    gboolean r;
    gchar *message;

    message = g_strdup_printf("END %s", id);
    r = libeventd_evp_context_send_message(self, message, error);
    g_free(message);

    return r;
}

gboolean
libeventd_evp_context_send_answered(LibeventdEvpContext *self, const gchar *id, const gchar *answer, GHashTable *data, GError **error)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(id != NULL, FALSE);
    g_return_val_if_fail(answer != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if ( self->out == NULL )
        /* Passive mode */
        return TRUE;

    gchar *message;

    message = g_strdup_printf(".ANSWERED %s %s", id, answer);
    if ( ! libeventd_evp_context_send_message(self, message, error) )
        goto fail;
    g_free(message);

    if ( ! _libeventd_evp_context_send_data(self, data, error) )
        return FALSE;

    if ( ! libeventd_evp_context_send_message(self, ".", error) )
        return FALSE;

    return TRUE;

fail:
    g_free(message);
    return FALSE;
}

gboolean
libeventd_evp_context_send_ended(LibeventdEvpContext *self, const gchar *id, EventdEventEndReason reason, GError **error)
{
    GEnumValue *reason_value = g_enum_get_value(g_type_class_ref(EVENTD_TYPE_EVENT_END_REASON), reason);
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(id != NULL, FALSE);
    g_return_val_if_fail(reason_value != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if ( self->out == NULL )
        /* Passive mode */
        return TRUE;

    gboolean r;
    gchar *message;

    message = g_strdup_printf("ENDED %s %s", id, reason_value->value_nick);
    r = libeventd_evp_context_send_message(self, message, error);
    g_free(message);

    return r;
}


void
libeventd_evp_context_send_bye(LibeventdEvpContext *self)
{
    g_return_if_fail(self != NULL);

    if ( self->out == NULL )
        /* Passive mode */
        return;

    libeventd_evp_context_send_message(self, "BYE", NULL);
}


gboolean
libeventd_evp_context_send_message(LibeventdEvpContext *self, const gchar *message, GError **error)
{
    GError *_inner_error_ = NULL;

    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(message != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);


#ifdef DEBUG
    g_debug("Sending line: %s", message);
#endif /* DEBUG */

    if ( ! g_data_output_stream_put_string(self->out, message, self->cancellable, &_inner_error_) )
    {
        g_propagate_error(error, _inner_error_);
        libeventd_evp_context_close(self);
        return FALSE;
    }
    if ( ! g_data_output_stream_put_byte(self->out, '\n', self->cancellable, &_inner_error_) )
    {
        g_propagate_error(error, _inner_error_);
        libeventd_evp_context_close(self);
        return FALSE;
    }

    return TRUE;
}
