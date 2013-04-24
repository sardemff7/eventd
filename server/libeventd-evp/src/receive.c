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
    gchar *id;
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
    if ( g_str_has_prefix(line, "DATA ") )
    {
        gchar **sline;

        sline = g_strsplit(line + strlen("DATA "), " ", 2);

        g_hash_table_insert(data_hash, sline[0], sline[1]);

        g_free(sline);

        _libeventd_evp_receive(self, callback, user_data);
    }
    else if ( g_str_has_prefix(line, ".DATA ") )
        _libeventd_evp_context_receive_data(self, data_hash, line + strlen(".DATA "), callback, user_data);
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
        if ( ! data->error )
        {
            if ( g_hash_table_size(data->data_hash) == 0 )
                g_hash_table_unref(data->data_hash);
            else
                eventd_event_set_all_data(data->event, data->data_hash);

            if ( self->interface->event != NULL )
                self->interface->event(self->client, self, data->id, data->event);
        }

        g_object_unref(data->event);
        g_free(data);

        _libeventd_evp_receive(self, _libeventd_evp_context_receive_callback, self);
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
        if ( self->interface->answered != NULL )
            self->interface->answered(self->client, self, data->id, data->answer, data->data_hash);

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
    else if ( g_str_has_prefix(line, ".EVENT ") )
    {
        gchar *id, *category = NULL, *name = NULL;
        id = line + strlen(".EVENT ");
        category = strchr(id, ' ');
        *category = '\0';
        category += strlen(" ");
        name = strchr(category, ' ');
        *name = '\0';
        name += strlen(" ");

        LibeventdEvpReceiveEventData *data;

        data = g_new0(LibeventdEvpReceiveEventData, 1);
        data->context = self;
        data->id = g_strdup(id);
        data->event = eventd_event_new(category, name);
        data->data_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

        _libeventd_evp_receive(self, _libeventd_evp_context_receive_event_callback, data);
        g_free(line);
        return;
    }
    else if ( g_str_has_prefix(line, "END ") )
    {
        if ( self->interface->end != NULL )
            self->interface->end(self->client, self, line + strlen("END "));
    }
    else if ( g_str_has_prefix(line, "ENDED ") )
    {
        gchar **end;

        end = g_strsplit(line + strlen("ENDED "), " ", 2);

        EventdEventEndReason reason = EVENTD_EVENT_END_REASON_NONE;
        GEnumValue *value = g_enum_get_value_by_nick(g_type_class_ref(EVENTD_TYPE_EVENT_END_REASON), end[1]);
        if ( value != NULL )
            reason = value->value;

        if ( self->interface->ended != NULL )
            self->interface->ended(self->client, self, end[0], reason);

        g_strfreev(end);
    }
    else if ( g_str_has_prefix(line, ".ANSWERED ") )
    {
        gchar **answer;

        answer = g_strsplit(line + strlen(".ANSWERED "), " ", 2);

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


    g_free(line);

    _libeventd_evp_receive(self, _libeventd_evp_context_receive_callback, self);
}

void
libeventd_evp_context_receive_loop(LibeventdEvpContext *self, gint priority)
{
    g_return_if_fail(self != NULL);

    self->priority = priority;

    g_cancellable_reset(self->cancellable);

    _libeventd_evp_receive(self, _libeventd_evp_context_receive_callback, self);
}
