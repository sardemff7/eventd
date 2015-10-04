/*
 * libeventd-protocol - Main eventd library - Protocol manipulation
 *
 * Copyright © 2011-2015 Quentin "Sardem FF7" Glidic
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <uuid.h>

#include <glib.h>
#include <glib-object.h>

#include <libeventd-event.h>
#include <libeventd-event-private.h>
#include <libeventd-protocol.h>

#include "protocol-private.h"
#include "protocol-evp-private.h"

static const gchar *_eventd_protocol_evp_states[_EVENTD_PROTOCOL_EVP_STATE_SIZE] = {
    [EVENTD_PROTOCOL_EVP_STATE_BASE]         = "base",
    [EVENTD_PROTOCOL_EVP_STATE_PASSIVE]      = "passive",
    [EVENTD_PROTOCOL_EVP_STATE_DOT_DATA]     = "dot message DATA",
    [EVENTD_PROTOCOL_EVP_STATE_DOT_EVENT]    = "dot message EVENT",
    [EVENTD_PROTOCOL_EVP_STATE_DOT_ANSWERED] = "dot message ANSWERED",
    [EVENTD_PROTOCOL_EVP_STATE_IGNORING]     = "ignoring",
};

typedef void (*EventdProtocolEvpTokenParseStartFunc)(EventdProtocolEvp *self, const gchar * const *argv, GError **error);
typedef gboolean (*EventdProtocolEvpTokenParseContinueFunc)(EventdProtocolEvp *self, const gchar *line, GError **error);
typedef void (*EventdProtocolEvpTokenParseStopFunc)(EventdProtocolEvp *self, GError **error);
typedef struct {
    const gchar *message;
    gsize min_args;
    gsize max_args;
    EventdProtocolEvpState start_states[_EVENTD_PROTOCOL_EVP_STATE_SIZE];
    EventdProtocolEvpTokenParseStartFunc start_func;
    EventdProtocolEvpState continue_state;
    EventdProtocolEvpTokenParseContinueFunc continue_func;
    EventdProtocolEvpTokenParseStopFunc stop_func;
} EventdProtocolEvpTokens;


static void
_eventd_protocol_evp_add_data(EventdProtocolEvp *self, gchar *name, gchar *value)
{
    if ( self->priv->data.hash == NULL )
        self->priv->data.hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(self->priv->data.hash, name, value);
}

static gboolean
_eventd_protocol_evp_parse_dot__continue_noeat(EventdProtocolEvp *self, const gchar *line, GError **error)
{
    return FALSE;
}

/* dot messages catch-all */
static void
_eventd_protocol_evp_parse_dot_catchall_start(EventdProtocolEvp *self, const gchar * const *argv, GError **error)
{
    self->priv->return_state = self->priv->state;
    ++self->priv->catchall.level;
    self->priv->state = EVENTD_PROTOCOL_EVP_STATE_IGNORING;
}

static gboolean
_eventd_protocol_evp_parse_dot_catchall_continue(EventdProtocolEvp *self, const gchar *line, GError **error)
{
    if ( g_str_has_prefix(line, ".") && ( ! g_str_has_prefix(line, "..") ) )
        ++self->priv->catchall.level;
    return TRUE;
}

static void
_eventd_protocol_evp_parse_dot_catchall_end(EventdProtocolEvp *self, GError **error)
{
    if ( --self->priv->catchall.level < 1 )
        self->priv->state = self->priv->return_state;
}

/* .DATA */
static void
_eventd_protocol_evp_parse_dot_data_start(EventdProtocolEvp *self, const gchar * const *argv, GError **error)
{
    self->priv->data.name = g_strdup(argv[0]);
    self->priv->data.value = g_string_new("");

    self->priv->data.return_state = self->priv->state;
    self->priv->state = EVENTD_PROTOCOL_EVP_STATE_DOT_DATA;
}

static gboolean
_eventd_protocol_evp_parse_dot_data_continue(EventdProtocolEvp *self, const gchar *line, GError **error)
{
    if ( g_str_has_prefix(line, "..") )
        ++line; /* Skip the first dot */
    else if ( g_str_has_prefix(line, ".") )
        g_set_error(error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_MALFORMED, "Malformed data line, starting with a single dot (expected two dots): %s", line);
    g_string_append_c(g_string_append(self->priv->data.value, line), '\n');
    return TRUE;
}

static void
_eventd_protocol_evp_parse_dot_data_end(EventdProtocolEvp *self, GError **error)
{
    /* Strip the last added newline */
    g_string_truncate(self->priv->data.value, self->priv->data.value->len - 1);

    _eventd_protocol_evp_add_data(self, self->priv->data.name, g_string_free(self->priv->data.value, FALSE));
    self->priv->data.name = NULL;
    self->priv->data.value = NULL;

    self->priv->state = self->priv->data.return_state;
}

static EventdEvent *
_eventd_protocol_evp_parser_get_event(EventdProtocolEvp *self, const gchar * const *argv, GError **error)
{
    uuid_t uuid;
    if ( uuid_parse(argv[0], uuid) < 0 )
    {
        g_set_error(error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_WRONG_UUID, "Error while parsing UUID %s", argv[0]);
        return NULL;
    }

    EventdEvent *event;
    event = eventd_event_new_for_uuid(uuid, argv[1], argv[2]);

    eventd_protocol_evp_add_event(self, event);

    return event;
}

/* .EVENT */
static void
_eventd_protocol_evp_parse_dot_event_start(EventdProtocolEvp *self, const gchar * const *argv, GError **error)
{
    self->priv->event = _eventd_protocol_evp_parser_get_event(self, argv, error);

    if ( self->priv->event == NULL )
        return _eventd_protocol_evp_parse_dot_catchall_start(self, argv, error);


    self->priv->return_state = self->priv->state;
    self->priv->state = EVENTD_PROTOCOL_EVP_STATE_DOT_EVENT;
}

static void
_eventd_protocol_evp_parse_dot_event_end(EventdProtocolEvp *self, GError **error)
{
    if ( self->priv->data.hash != NULL )
    {
        eventd_event_set_all_data(self->priv->event, self->priv->data.hash);
        self->priv->data.hash = NULL;
    }

    g_signal_emit(self, _eventd_protocol_signals[SIGNAL_EVENT], 0, self->priv->event);

    g_object_unref(self->priv->event);
    self->priv->event = NULL;

    self->priv->state = self->priv->return_state;
}

/* .ANSWERED */
static void
_eventd_protocol_evp_parse_dot_answered_start(EventdProtocolEvp *self, const gchar * const *argv, GError **error)
{
    self->priv->answer.id = g_strdup(argv[0]);
    self->priv->answer.answer = g_strdup(argv[1]);

    self->priv->return_state = self->priv->state;
    self->priv->state = EVENTD_PROTOCOL_EVP_STATE_DOT_ANSWERED;
}

static void
_eventd_protocol_evp_parse_dot_answered_end(EventdProtocolEvp *self, GError **error)
{
    EventdEvent *event;
    event = g_hash_table_lookup(self->priv->events, self->priv->answer.id);
    if ( event != NULL )
    {
        eventd_event_set_all_answer_data(event, self->priv->data.hash);
        g_signal_emit(self, _eventd_protocol_signals[SIGNAL_ANSWERED], 0, event, self->priv->answer.answer);
    }
    else if ( self->priv->data.hash != NULL )
        g_hash_table_unref(self->priv->data.hash);

    g_free(self->priv->answer.id);
    g_free(self->priv->answer.answer);

    self->priv->answer.id = NULL;
    self->priv->answer.answer = NULL;
    self->priv->data.hash = NULL;

    self->priv->state = self->priv->return_state;
}

/* DATA */
static void
_eventd_protocol_evp_parse_data(EventdProtocolEvp *self, const gchar * const *argv, GError **error)
{
    _eventd_protocol_evp_add_data(self, g_strdup(argv[0]), g_strdup(argv[1]));
}

/* EVENT */
static void
_eventd_protocol_evp_parse_event(EventdProtocolEvp *self, const gchar * const *argv, GError **error)
{
    EventdEvent *event;

    event = _eventd_protocol_evp_parser_get_event(self, argv, error);
    if ( event == NULL )
        return;
    g_signal_emit(self, _eventd_protocol_signals[SIGNAL_EVENT], 0, event);
    g_object_unref(event);
}

/* ANSWER */
static void
_eventd_protocol_evp_parse_answer(EventdProtocolEvp *self, const gchar * const *argv, GError **error)
{
    eventd_event_add_answer(self->priv->event, argv[0]);
}

/* ENDED */
static void
_eventd_protocol_evp_parse_ended(EventdProtocolEvp *self, const gchar * const *argv, GError **error)
{
    EventdEvent *event;
    event = g_hash_table_lookup(self->priv->events, argv[0]);
    if ( event == NULL )
        return;

    /*
     * For forward-compatibility, we use the 'unknown' reason if we
     * do not have the real one at compile time
     */
    EventdEventEndReason reason = EVENTD_EVENT_END_REASON_UNKNOWN;
    GEnumClass *enum_class;
    GEnumValue *enum_value;

    enum_class = g_type_class_ref(EVENTD_TYPE_EVENT_END_REASON);
    enum_value = g_enum_get_value_by_nick(enum_class, argv[1]);
    if ( enum_value != NULL )
        reason = enum_value->value;
    g_type_class_unref(enum_class);

    g_signal_emit(self, _eventd_protocol_signals[SIGNAL_ENDED], 0, event, reason);

    eventd_protocol_evp_remove_event(self, event);
}

/* PASSIVE */
static void
_eventd_protocol_evp_parse_passive(EventdProtocolEvp *self, const gchar * const *argv, GError **error)
{
    g_signal_emit(self, _eventd_protocol_signals[SIGNAL_PASSIVE], 0);
    self->priv->state = EVENTD_PROTOCOL_EVP_STATE_PASSIVE;
}

/* BYE */
static void
_eventd_protocol_evp_parse_bye(EventdProtocolEvp *self, const gchar * const *argv, GError **error)
{
    g_signal_emit(self, _eventd_protocol_signals[SIGNAL_BYE], 0, ( argv == NULL ) ? NULL : argv[0]);
}

static const EventdProtocolEvpTokens _eventd_protocol_evp_dot_messages[] = {
    {"DATA", 1, 1,
            { EVENTD_PROTOCOL_EVP_STATE_DOT_EVENT, EVENTD_PROTOCOL_EVP_STATE_DOT_ANSWERED, _EVENTD_PROTOCOL_EVP_STATE_SIZE },
            _eventd_protocol_evp_parse_dot_data_start,
            EVENTD_PROTOCOL_EVP_STATE_DOT_DATA,
            _eventd_protocol_evp_parse_dot_data_continue,
            _eventd_protocol_evp_parse_dot_data_end
    },
    {"EVENT", 3, 3,
            { EVENTD_PROTOCOL_EVP_STATE_BASE, EVENTD_PROTOCOL_EVP_STATE_PASSIVE,_EVENTD_PROTOCOL_EVP_STATE_SIZE },
            _eventd_protocol_evp_parse_dot_event_start,
            EVENTD_PROTOCOL_EVP_STATE_DOT_EVENT,
            _eventd_protocol_evp_parse_dot__continue_noeat,
            _eventd_protocol_evp_parse_dot_event_end
    },
    {"ANSWERED", 2, 2,
            { EVENTD_PROTOCOL_EVP_STATE_BASE, EVENTD_PROTOCOL_EVP_STATE_PASSIVE,_EVENTD_PROTOCOL_EVP_STATE_SIZE },
            _eventd_protocol_evp_parse_dot_answered_start,
            EVENTD_PROTOCOL_EVP_STATE_DOT_ANSWERED,
            _eventd_protocol_evp_parse_dot__continue_noeat,
            _eventd_protocol_evp_parse_dot_answered_end
    },
    /* Catch-all message to ignore future dot messages */
    {"", 0, G_MAXSIZE,
            { EVENTD_PROTOCOL_EVP_STATE_BASE, EVENTD_PROTOCOL_EVP_STATE_PASSIVE,_EVENTD_PROTOCOL_EVP_STATE_SIZE },
            _eventd_protocol_evp_parse_dot_catchall_start,
            EVENTD_PROTOCOL_EVP_STATE_IGNORING,
            _eventd_protocol_evp_parse_dot_catchall_continue,
            _eventd_protocol_evp_parse_dot_catchall_end
    },
    { NULL }
};

static const EventdProtocolEvpTokens _eventd_protocol_evp_messages[] = {
    {"DATA", 2, 2,
            { EVENTD_PROTOCOL_EVP_STATE_DOT_EVENT, EVENTD_PROTOCOL_EVP_STATE_DOT_ANSWERED, _EVENTD_PROTOCOL_EVP_STATE_SIZE },
            _eventd_protocol_evp_parse_data,
            _EVENTD_PROTOCOL_EVP_STATE_SIZE, NULL, NULL
    },
    {"EVENT", 3, 3,
            { EVENTD_PROTOCOL_EVP_STATE_BASE, EVENTD_PROTOCOL_EVP_STATE_PASSIVE,_EVENTD_PROTOCOL_EVP_STATE_SIZE },
            _eventd_protocol_evp_parse_event,
            _EVENTD_PROTOCOL_EVP_STATE_SIZE, NULL, NULL
    },
    {"ANSWER", 1, 1,
            { EVENTD_PROTOCOL_EVP_STATE_DOT_EVENT, _EVENTD_PROTOCOL_EVP_STATE_SIZE },
            _eventd_protocol_evp_parse_answer,
            _EVENTD_PROTOCOL_EVP_STATE_SIZE, NULL, NULL
    },
    {"ENDED", 2, 2,
            { EVENTD_PROTOCOL_EVP_STATE_BASE, EVENTD_PROTOCOL_EVP_STATE_PASSIVE, _EVENTD_PROTOCOL_EVP_STATE_SIZE },
            _eventd_protocol_evp_parse_ended,
            _EVENTD_PROTOCOL_EVP_STATE_SIZE, NULL, NULL
    },
    {"PASSIVE", 0, 0,
            { EVENTD_PROTOCOL_EVP_STATE_BASE, _EVENTD_PROTOCOL_EVP_STATE_SIZE },
            _eventd_protocol_evp_parse_passive,
            _EVENTD_PROTOCOL_EVP_STATE_SIZE, NULL, NULL
    },
    {"BYE", 0, 1,
            { EVENTD_PROTOCOL_EVP_STATE_BASE, EVENTD_PROTOCOL_EVP_STATE_PASSIVE, _EVENTD_PROTOCOL_EVP_STATE_SIZE },
            _eventd_protocol_evp_parse_bye,
            _EVENTD_PROTOCOL_EVP_STATE_SIZE, NULL, NULL
    },
    { NULL }
};

static void
_eventd_protocol_evp_parse_line(EventdProtocolEvp *self, const gchar *line, GError **error)
{
    g_debug("[%s] Parse line: %s", _eventd_protocol_evp_states[self->priv->state], line);
    const EventdProtocolEvpTokens *message;

    /*
     * Handle the end of a dot message
     */
    if ( g_strcmp0(line, ".") == 0 )
    {
        for ( message = _eventd_protocol_evp_dot_messages ; message->message != NULL ; ++message )
        {
            if ( self->priv->state == message->continue_state )
                return message->stop_func(self, error);
        }
        return g_set_error(error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_UNEXPECTED_TOKEN, "Got '.' in an invalid state '%s'", _eventd_protocol_evp_states[self->priv->state]);
    }

    /*
     * Handle dot message line
     */
    for ( message = _eventd_protocol_evp_dot_messages ; message->message != NULL ; ++message )
    {
        if ( self->priv->state == message->continue_state )
        {
            if ( message->continue_func(self, line, error) )
                return;
        }
    }

    /*
     * Either we got a brand new message
     * or the dot message did not eat the line
     */

    const EventdProtocolEvpState *state;
    if ( g_str_has_prefix(line, ".") )
    {
        message = _eventd_protocol_evp_dot_messages;
        ++line;
    }
    else
        message = _eventd_protocol_evp_messages;

    for ( ; message->message != NULL ; ++message )
    {
        if ( ! g_str_has_prefix(line, message->message) )
            continue;
        const gchar *args = line + strlen(message->message);
        gchar **argv = NULL;
        if ( g_str_has_prefix(args, " ") )
        {
            ++args;
            if ( message->max_args < 1 )
                return g_set_error(error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_MALFORMED, "Message '%s' does not take arguments, but got '%s'", message->message, args);

            gsize argc;
            argv = g_strsplit(args, " ", message->max_args);
            argc = g_strv_length(argv);
            if ( argc < message->min_args )
            {
                g_strfreev(argv);
                return g_set_error(error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_MALFORMED, "Message '%s' does take at least %zd arguments, but got %zd", message->message, message->min_args, argc);
            }
        }
        else if ( message->min_args > 0 )
            return g_set_error(error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_MALFORMED, "Message '%s' does take at least %zd arguments, but got none", message->message, message->min_args);

        gboolean valid = FALSE;
        for ( state = message->start_states ; *state != _EVENTD_PROTOCOL_EVP_STATE_SIZE ; ++state )
        {
            if ( self->priv->state == *state )
                valid = TRUE;
        }
        if ( valid )
            message->start_func(self, (const gchar * const *) argv, error);
        else
            g_set_error(error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_UNEXPECTED_TOKEN, "Message '%s' in an invalid state '%s'", message->message, _eventd_protocol_evp_states[self->priv->state]);
        return;
    }
}

gboolean
eventd_protocol_evp_parse(EventdProtocol *protocol, gchar **buffer, GError **error)
{
    g_return_val_if_fail(EVENTD_IS_PROTOCOL_EVP(protocol), FALSE);
    EventdProtocolEvp *self = EVENTD_PROTOCOL_EVP(protocol);

    GError *_inner_error_ = NULL;

    gchar *l;
    l = g_utf8_strchr(*buffer, -1, '\n');
    if ( l == NULL )
        _eventd_protocol_evp_parse_line(self, *buffer, &_inner_error_);
    else
        g_set_error_literal(&_inner_error_, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_GARBAGE, "EvP parser only takes lines");

    g_free(*buffer);
    *buffer = NULL;

    if ( _inner_error_ == NULL )
        return TRUE;

    g_propagate_error(error, _inner_error_);
    return FALSE;
}
