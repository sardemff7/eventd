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
#include <libeventd-event-private.h>

#include <libeventd-evp.h>

#include "context.h"

static void _libeventd_evp_context_receive_data_callback(GObject *source_object, GAsyncResult *res, gpointer user_data);
static void _libeventd_evp_context_receive_event_callback(GObject *source_object, GAsyncResult *res, gpointer user_data);
static void _libeventd_evp_context_receive_answered_callback(GObject *source_object, GAsyncResult *res, gpointer user_data);
static void _libeventd_evp_context_receive_handshake_callback(GObject *source_object, GAsyncResult *res, gpointer user_data);
static void _libeventd_evp_context_receive_callback(GObject *source_object, GAsyncResult *res, gpointer user_data);
static void
_libeventd_evp_receive(LibeventdEvpContext *self, GAsyncReadyCallback callback, gpointer user_data)
{
    if ( self->error != NULL )
    {
        self->interface->error(self->client, self, self->error);
        self->error = NULL;
        return;
    }
    g_data_input_stream_read_upto_async(self->in, "\n", -1, self->priority, self->cancellable, callback, user_data);
}

static gchar *
_libeventd_evp_receive_finish(LibeventdEvpContext *self, GAsyncResult *res)
{
    g_return_val_if_fail(G_IS_ASYNC_RESULT(res), NULL);
    gchar *line;

    line = g_data_input_stream_read_upto_finish(self->in, res, NULL, &self->error);
    if ( line == NULL )
    {
        if ( ( self->error != NULL ) && ( self->error->code == G_IO_ERROR_CANCELLED ) )
            g_clear_error(&self->error);
        return NULL;
    }

    if ( ! g_data_input_stream_read_byte(self->in, NULL, &self->error) )
        line = (g_free(line), NULL);

    return line;
}

typedef struct {
    LibeventdEvpContext *context;
    GHashTable *data_hash;
    gchar *name;
    GString *data;
    GAsyncReadyCallback callback;
    gpointer user_data;
} LibeventdEvpReceiveDataData;

typedef struct {
    LibeventdEvpContext *context;
    EventdEvent *event;
    GHashTable *data_hash;
    gboolean error;
} LibeventdEvpReceiveEventData;

typedef struct {
    LibeventdEvpContext *context;
    gchar *id;
    gchar *answer;
    GHashTable *data_hash;
    gboolean error;
} LibeventdEvpReceiveAnsweredData;

static void
_libeventd_evp_context_receive_data_continue(gpointer user_data)
{
    LibeventdEvpReceiveDataData *data = user_data;

    _libeventd_evp_receive(data->context, _libeventd_evp_context_receive_data_callback, data);
}

static void
_libeventd_evp_context_receive_data_callback(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    LibeventdEvpReceiveDataData *data = user_data;
    LibeventdEvpContext *self = data->context;

    gchar *line;

    line = _libeventd_evp_receive_finish(self, res);
    if ( line == NULL )
        return;

#ifdef DEBUG
    g_debug("Received DATA line: %s", line);
#endif /* DEBUG */

    if ( g_strcmp0(line, ".") == 0 )
    {
        g_hash_table_insert(data->data_hash, data->name, g_string_free(data->data, FALSE));

        g_hash_table_unref(data->data_hash);

        _libeventd_evp_receive(data->context, data->callback, data->user_data);

        g_free(data);
    }
    else
    {
        if ( *data->data->str != '\0' )
            data->data = g_string_append_c(data->data, '\n');
        data->data = g_string_append(data->data, ( line[0] == '.' ) ? ( line+1 ) : line);

        _libeventd_evp_context_receive_data_continue(data);
    }

    g_free(line);
}

static void
_libeventd_evp_context_receive_data(LibeventdEvpContext *self, GHashTable *data_hash, const gchar *name, GAsyncReadyCallback callback, gpointer user_data)
{
    LibeventdEvpReceiveDataData *data;

    data = g_new0(LibeventdEvpReceiveDataData, 1);
    data->context = self;
    data->data_hash = g_hash_table_ref(data_hash);
    data->name = g_strdup(name);
    data->data = g_string_new("");
    data->callback = callback;
    data->user_data = user_data;

    _libeventd_evp_context_receive_data_continue(data);
}

static gboolean
_libeventd_evp_context_receive_data_handle(LibeventdEvpContext *self, GHashTable *data_hash, const gchar *line, GAsyncReadyCallback callback, gpointer user_data)
{
    if ( g_str_has_prefix(line, "DATAL ") )
    {
        gchar **sline;

        sline = g_strsplit(line + strlen("DATAL "), " ", 2);

        g_hash_table_insert(data_hash, sline[0], sline[1]);

        g_free(sline);

        _libeventd_evp_receive(self, callback, user_data);
    }
    else if ( g_str_has_prefix(line, "DATA ") )
        _libeventd_evp_context_receive_data(self, data_hash, line + strlen("DATA "), callback, user_data);
    else
        return FALSE;
    return TRUE;
}

static void
_libeventd_evp_context_receive_event_callback(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    LibeventdEvpReceiveEventData *data = user_data;
    LibeventdEvpContext *self = data->context;

    gchar *line;

    line = _libeventd_evp_receive_finish(self, res);
    if ( line == NULL )
        return;

#ifdef DEBUG
    g_debug("Received EVENT line: %s", line);
#endif /* DEBUG */

    if ( g_strcmp0(line, ".") == 0 )
    {
        if ( data->error )
        {
            if ( ! libeventd_evp_context_send_message(self, "ERROR bad-event", &self->error) )
            {
                self->interface->error(self->client, self, self->error);
                self->error = NULL;
                return;
            }
        }
        else
        {
            gchar *id;
            gchar *message;

            if ( g_hash_table_size(data->data_hash) == 0 )
                g_hash_table_unref(data->data_hash);
            else
                eventd_event_set_all_data(data->event, data->data_hash);

            id = self->interface->event(self->client, self, data->event);
            message = g_strdup_printf("EVENT %s", id);
            g_free(id);

            if ( ! libeventd_evp_context_send_message(self, message, &self->error) )
            {
                self->interface->error(self->client, self, self->error);
                self->error = NULL;
                return;
            }

            g_free(message);
        }

        g_object_unref(data->event);
        g_free(data);

        _libeventd_evp_receive(self, _libeventd_evp_context_receive_callback, self);
    }
    else if ( g_str_has_prefix(line, "CATEGORY ") )
    {
        eventd_event_set_category(data->event, line + strlen("CATEGORY "));
        _libeventd_evp_receive(self, _libeventd_evp_context_receive_event_callback, data);
    }
    else if ( g_str_has_prefix(line, "ANSWER ") )
    {
        eventd_event_add_answer(data->event, line + strlen("ANSWER "));
        _libeventd_evp_receive(self, _libeventd_evp_context_receive_event_callback, data);
    }
    else if ( ! _libeventd_evp_context_receive_data_handle(self, data->data_hash, line, _libeventd_evp_context_receive_event_callback, data) )
    {
        data->error = TRUE;
        _libeventd_evp_receive(self, _libeventd_evp_context_receive_event_callback, data);
    }


    g_free(line);
}

static void
_libeventd_evp_context_receive_answered_callback(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    LibeventdEvpReceiveAnsweredData *data = user_data;
    LibeventdEvpContext *self = data->context;

    gchar *line;

    line = _libeventd_evp_receive_finish(self, res);
    if ( line == NULL )
        return;

#ifdef DEBUG
    g_debug("Received ANSWERED line: %s", line);
#endif /* DEBUG */

    if ( g_strcmp0(line, ".") == 0 )
    {
        gpointer event;
        event = self->interface->get_event(self->client, self, data->id);
        if ( event != NULL )
            self->interface->answered(self->client, self, event, data->answer, data->data_hash);

        g_free(data->answer);
        g_free(data->id);

        _libeventd_evp_receive(self, _libeventd_evp_context_receive_callback, self);

        g_free(data);
    }
    else if ( ! _libeventd_evp_context_receive_data_handle(self, data->data_hash, line, _libeventd_evp_context_receive_answered_callback, data) )
        data->error = TRUE;

    g_free(line);
}

/*
 * General receiving callbacks
 */
static void
_libeventd_evp_context_receive_handshake_callback(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    LibeventdEvpContext *self = user_data;

    gchar *line;

    line = _libeventd_evp_receive_finish(self, res);
    if ( line == NULL )
        return;

#ifdef DEBUG
    g_debug("Received handshake line: %s", line);
#endif /* DEBUG */

    /* We proccess messages … */
    if ( g_str_has_prefix(line, "HELLO ") )
    {
        if ( ! libeventd_evp_context_send_message(self, "HELLO", &self->error) )
        {
            self->interface->error(self->client, self, self->error);
            self->error = NULL;
            return;
        }
        self->interface->hello(self->client, self, line + strlen("HELLO "));

        _libeventd_evp_receive(self, _libeventd_evp_context_receive_callback, self);
    }
    /* … then warn if we received something unexpected */
    else
    {
        if ( ! libeventd_evp_context_send_message(self, "ERROR bad-handshake", &self->error) )
        {
            self->interface->error(self->client, self, self->error);
            self->error = NULL;
            return;
        }
        _libeventd_evp_receive(self, _libeventd_evp_context_receive_handshake_callback, self);
    }

    g_free(line);
}

static void
_libeventd_evp_context_receive_callback(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    LibeventdEvpContext *self = user_data;

    gchar *line;

    line = _libeventd_evp_receive_finish(self, res);
    if ( line == NULL )
        return;

#ifdef DEBUG
    g_debug("Received line: %s", line);
#endif /* DEBUG */

    /* We proccess messages … */
    if ( g_strcmp0(line, "BYE") == 0 )
    {
        g_free(line);
        self->interface->bye(self->client, self);
        return;
    }
    else if ( self->server && g_str_has_prefix(line, "EVENT ") )
    {
        LibeventdEvpReceiveEventData *data;

        data = g_new0(LibeventdEvpReceiveEventData, 1);
        data->context = self;
        data->event = eventd_event_new(line + strlen("EVENT "));
        data->data_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

        _libeventd_evp_receive(self, _libeventd_evp_context_receive_event_callback, data);
        g_free(line);
        return;
    }
    else if ( self->server && g_str_has_prefix(line, "END ") )
    {
        const gchar *id;
        gpointer event;

        id = line + strlen("END ");
        event = self->interface->get_event(self->client, self, id);
        if ( event != NULL )
        {
            gchar *message;

            message = g_strdup_printf("ENDING %s", id);

            if ( ! libeventd_evp_context_send_message(self, message, &self->error) )
            {
                self->interface->error(self->client, self, self->error);
                self->error = NULL;
                return;
            }

            self->interface->end(self->client, self, event);

            g_free(message);
        }
        else if ( ! libeventd_evp_context_send_message(self, "ERROR bad-id", &self->error) )
        {
            self->interface->error(self->client, self, self->error);
            self->error = NULL;
            return;
        }
    }
    else if ( g_str_has_prefix(line, "ENDED ") )
    {
        gchar **end;
        gpointer event;

        end = g_strsplit(line + strlen("ENDED "), " ", 2);

        event = self->interface->get_event(self->client, self, end[0]);
        if ( event != NULL )
        {
            EventdEventEndReason reason = EVENTD_EVENT_END_REASON_NONE;

             if ( g_strcmp0(end[1], "timeout") == 0 )
                reason = EVENTD_EVENT_END_REASON_TIMEOUT;
            else if ( g_strcmp0(end[1], "user-dismiss") == 0 )
                reason = EVENTD_EVENT_END_REASON_USER_DISMISS;
            else if ( g_strcmp0(end[1], "client-dismiss") == 0 )
                reason = EVENTD_EVENT_END_REASON_CLIENT_DISMISS;
            else if ( g_strcmp0(end[1], "reserved") == 0 )
                reason = EVENTD_EVENT_END_REASON_RESERVED;

            self->interface->ended(self->client, self, event, reason);
        }

        g_strfreev(end);
    }
    else if ( g_str_has_prefix(line, "ANSWERED ") )
    {
        gchar **answer;

        answer = g_strsplit(line + strlen("ANSWERED "), " ", 2);

        LibeventdEvpReceiveAnsweredData *data;

        data = g_new0(LibeventdEvpReceiveAnsweredData, 1);
        data->context = self;
        data->id = answer[0];
        data->data_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        data->answer = answer[1];

        _libeventd_evp_receive(self, _libeventd_evp_context_receive_answered_callback, data);

        g_free(answer);
        return;
    }
    /* … and just inform for answers … */
    else if ( self->waiter.callback != NULL )
    {
        self->waiter.callback(self, line);
        self->waiter.callback = NULL;
        self->waiter.result = NULL;
        self->waiter.id = NULL;
    }
    /* … then warn if we received something unexpected */
    else if ( ! libeventd_evp_context_send_message(self, "ERROR unknown", &self->error) )
    {
        self->interface->error(self->client, self, self->error);
        self->error = NULL;
        return;
    }


    g_free(line);

    _libeventd_evp_receive(self, _libeventd_evp_context_receive_callback, self);
}

void
libeventd_evp_context_receive_loop_client(LibeventdEvpContext *self, gint priority)
{
    g_return_if_fail(self != NULL);

    self->priority = priority;

    g_cancellable_reset(self->cancellable);

    self->server = FALSE;

    _libeventd_evp_receive(self, _libeventd_evp_context_receive_callback, self);
}

void
libeventd_evp_context_receive_loop_server(LibeventdEvpContext *self, gint priority)
{
    g_return_if_fail(self != NULL);

    self->priority = priority;

    g_cancellable_reset(self->cancellable);

    self->server = TRUE;

    _libeventd_evp_receive(self, _libeventd_evp_context_receive_handshake_callback, self);
}
