/*
 * libeventd-protocol - Main eventd library - Protocol manipulation
 *
 * Copyright © 2011-2016 Quentin "Sardem FF7" Glidic
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

#include <glib.h>
#include <glib-object.h>

#include <libeventd-event.h>
#include <libeventd-event-private.h>
#include <libeventd-protocol.h>

#include "protocol-private.h"
#include "protocol-evp-private.h"

static const gchar *_eventd_protocol_evp_states[_EVENTD_PROTOCOL_EVP_STATE_SIZE] = {
    [EVENTD_PROTOCOL_EVP_STATE_BASE]          = "base",
    [EVENTD_PROTOCOL_EVP_STATE_SUBSCRIBE]     = "subscribe",
    [EVENTD_PROTOCOL_EVP_STATE_BYE]           = "bye",
    [EVENTD_PROTOCOL_EVP_STATE_DOT_DATA]      = "dot message DATA",
    [EVENTD_PROTOCOL_EVP_STATE_DOT_EVENT]     = "dot message EVENT",
    [EVENTD_PROTOCOL_EVP_STATE_DOT_SUBSCRIBE] = "dot message SUBSCRIBE",
    [EVENTD_PROTOCOL_EVP_STATE_IGNORING]      = "ignoring",
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
    if ( self->data.hash == NULL )
        self->data.hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(self->data.hash, name, value);
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
    ++self->catchall.level;
    self->state = EVENTD_PROTOCOL_EVP_STATE_IGNORING;
}

static gboolean
_eventd_protocol_evp_parse_dot_catchall_continue(EventdProtocolEvp *self, const gchar *line, GError **error)
{
    if ( g_str_has_prefix(line, ".") && ( ! g_str_has_prefix(line, "..") ) )
        ++self->catchall.level;
    return TRUE;
}

static void
_eventd_protocol_evp_parse_dot_catchall_end(EventdProtocolEvp *self, GError **error)
{
    if ( --self->catchall.level < 1 )
        self->state = self->base_state;
}

/* .DATA */
static void
_eventd_protocol_evp_parse_dot_data_start(EventdProtocolEvp *self, const gchar * const *argv, GError **error)
{
    self->data.name = g_strdup(argv[0]);
    self->data.value = g_string_new("");

    self->data.return_state = self->state;
    self->state = EVENTD_PROTOCOL_EVP_STATE_DOT_DATA;
}

static gboolean
_eventd_protocol_evp_parse_dot_data_continue(EventdProtocolEvp *self, const gchar *line, GError **error)
{
    if ( g_str_has_prefix(line, "..") )
        ++line; /* Skip the first dot */
    else if ( g_str_has_prefix(line, ".") )
        g_set_error(error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_MALFORMED, "Malformed data line, starting with a single dot (expected two dots): %s", line);
    g_string_append_c(g_string_append(self->data.value, line), '\n');
    return TRUE;
}

static void
_eventd_protocol_evp_parse_dot_data_end(EventdProtocolEvp *self, GError **error)
{
    /* Strip the last added newline */
    g_string_truncate(self->data.value, self->data.value->len - 1);

    _eventd_protocol_evp_add_data(self, self->data.name, g_string_free(self->data.value, FALSE));
    self->data.name = NULL;
    self->data.value = NULL;

    self->state = self->data.return_state;
}

static EventdEvent *
_eventd_protocol_evp_parser_get_event(EventdProtocolEvp *self, const gchar * const *argv, GError **error)
{
    EventdEvent *event;
    event = eventd_event_new_for_uuid_string(argv[0], argv[1], argv[2]);

    if ( event == NULL )
        g_set_error(error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_WRONG_UUID, "Error while parsing UUID %s", argv[0]);

    return event;
}

/* .EVENT */
static void
_eventd_protocol_evp_parse_dot_event_start(EventdProtocolEvp *self, const gchar * const *argv, GError **error)
{
    self->event = _eventd_protocol_evp_parser_get_event(self, argv, error);

    if ( self->event == NULL )
        return _eventd_protocol_evp_parse_dot_catchall_start(self, argv, error);


    self->state = EVENTD_PROTOCOL_EVP_STATE_DOT_EVENT;
}

static void
_eventd_protocol_evp_parse_dot_event_end(EventdProtocolEvp *self, GError **error)
{
    if ( self->data.hash != NULL )
    {
        eventd_event_set_all_data(self->event, self->data.hash);
        self->data.hash = NULL;
    }

    eventd_protocol_call_event((EventdProtocol *) self, self->event);

    eventd_event_unref(self->event);
    self->event = NULL;

    self->state = self->base_state;
}

/* .SUBSCRIBE */
static void
_eventd_protocol_evp_parse_dot_subscribe_start(EventdProtocolEvp *self, const gchar * const *argv, GError **error)
{
    self->subscriptions = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    self->state = EVENTD_PROTOCOL_EVP_STATE_DOT_SUBSCRIBE;
    self->base_state = EVENTD_PROTOCOL_EVP_STATE_SUBSCRIBE;
}

static gboolean
_eventd_protocol_evp_parse_dot_subscribe_continue(EventdProtocolEvp *self, const gchar *line, GError **error)
{
    g_hash_table_add(self->subscriptions, g_strdup(line));
    return TRUE;
}

static void
_eventd_protocol_evp_parse_dot_subscribe_end(EventdProtocolEvp *self, GError **error)
{
    if ( g_hash_table_size(self->subscriptions) < 2 )
        return g_set_error(error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_MALFORMED, "SUBSCRIBE dot message requires at least two categories");

    eventd_protocol_call_subscribe((EventdProtocol *) self, self->subscriptions);

    g_hash_table_unref(self->subscriptions);
    self->subscriptions = NULL;

    self->state = self->base_state;
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
    eventd_protocol_call_event((EventdProtocol *) self, event);
    eventd_event_unref(event);
}

/* SUBSCRIBE */
static void
_eventd_protocol_evp_parse_subscribe(EventdProtocolEvp *self, const gchar * const *argv, GError **error)
{
    if ( argv == NULL )
        eventd_protocol_call_subscribe((EventdProtocol *) self, NULL);
    else
    {
        GHashTable *subscriptions;
        subscriptions = g_hash_table_new(g_str_hash, g_str_equal);
        g_hash_table_add(subscriptions, (gpointer) argv[0]);
        eventd_protocol_call_subscribe((EventdProtocol *) self, subscriptions);
        g_hash_table_unref(subscriptions);
    }

    self->base_state = EVENTD_PROTOCOL_EVP_STATE_SUBSCRIBE;
    self->state = self->base_state;
}

/* BYE */
static void
_eventd_protocol_evp_parse_bye(EventdProtocolEvp *self, const gchar * const *argv, GError **error)
{
    eventd_protocol_call_bye((EventdProtocol *) self, ( argv == NULL ) ? NULL : argv[0]);

    self->base_state = EVENTD_PROTOCOL_EVP_STATE_BYE;
    self->state = self->base_state;
}

static const EventdProtocolEvpTokens _eventd_protocol_evp_dot_messages[] = {
    {"DATA", 1, 1,
            { EVENTD_PROTOCOL_EVP_STATE_DOT_EVENT, _EVENTD_PROTOCOL_EVP_STATE_SIZE },
            _eventd_protocol_evp_parse_dot_data_start,
            EVENTD_PROTOCOL_EVP_STATE_DOT_DATA,
            _eventd_protocol_evp_parse_dot_data_continue,
            _eventd_protocol_evp_parse_dot_data_end
    },
    {"EVENT", 3, 3,
            { EVENTD_PROTOCOL_EVP_STATE_BASE, EVENTD_PROTOCOL_EVP_STATE_SUBSCRIBE, _EVENTD_PROTOCOL_EVP_STATE_SIZE },
            _eventd_protocol_evp_parse_dot_event_start,
            EVENTD_PROTOCOL_EVP_STATE_DOT_EVENT,
            _eventd_protocol_evp_parse_dot__continue_noeat,
            _eventd_protocol_evp_parse_dot_event_end
    },
    {"SUBSCRIBE", 0, 0,
            { EVENTD_PROTOCOL_EVP_STATE_BASE, _EVENTD_PROTOCOL_EVP_STATE_SIZE },
            _eventd_protocol_evp_parse_dot_subscribe_start,
            EVENTD_PROTOCOL_EVP_STATE_DOT_SUBSCRIBE,
            _eventd_protocol_evp_parse_dot_subscribe_continue,
            _eventd_protocol_evp_parse_dot_subscribe_end
    },
    /* Catch-all message to ignore future dot messages */
    {"", 0, G_MAXSIZE,
            { EVENTD_PROTOCOL_EVP_STATE_BASE, EVENTD_PROTOCOL_EVP_STATE_SUBSCRIBE, _EVENTD_PROTOCOL_EVP_STATE_SIZE },
            _eventd_protocol_evp_parse_dot_catchall_start,
            EVENTD_PROTOCOL_EVP_STATE_IGNORING,
            _eventd_protocol_evp_parse_dot_catchall_continue,
            _eventd_protocol_evp_parse_dot_catchall_end
    },
    { NULL }
};

static const EventdProtocolEvpTokens _eventd_protocol_evp_messages[] = {
    {"DATA", 2, 2,
            { EVENTD_PROTOCOL_EVP_STATE_DOT_EVENT, _EVENTD_PROTOCOL_EVP_STATE_SIZE },
            _eventd_protocol_evp_parse_data,
            _EVENTD_PROTOCOL_EVP_STATE_SIZE, NULL, NULL
    },
    {"EVENT", 3, 3,
            { EVENTD_PROTOCOL_EVP_STATE_BASE, EVENTD_PROTOCOL_EVP_STATE_SUBSCRIBE, _EVENTD_PROTOCOL_EVP_STATE_SIZE },
            _eventd_protocol_evp_parse_event,
            _EVENTD_PROTOCOL_EVP_STATE_SIZE, NULL, NULL
    },
    {"SUBSCRIBE", 0, 1,
            { EVENTD_PROTOCOL_EVP_STATE_BASE, _EVENTD_PROTOCOL_EVP_STATE_SIZE },
            _eventd_protocol_evp_parse_subscribe,
            _EVENTD_PROTOCOL_EVP_STATE_SIZE, NULL, NULL
    },
    {"BYE", 0, 1,
            { EVENTD_PROTOCOL_EVP_STATE_BASE, EVENTD_PROTOCOL_EVP_STATE_SUBSCRIBE, _EVENTD_PROTOCOL_EVP_STATE_SIZE },
            _eventd_protocol_evp_parse_bye,
            _EVENTD_PROTOCOL_EVP_STATE_SIZE, NULL, NULL
    },
    { NULL }
};

static void
_eventd_protocol_evp_parse_line(EventdProtocolEvp *self, const gchar *line, GError **error)
{
#ifdef EVENTD_DEBUG
    g_debug("[%s] Parse line: %s", _eventd_protocol_evp_states[self->state], line);
#endif /* EVENTD_DEBUG */

    const EventdProtocolEvpTokens *message;

    /*
     * Handle the end of a dot message
     */
    if ( g_strcmp0(line, ".") == 0 )
    {
        for ( message = _eventd_protocol_evp_dot_messages ; message->message != NULL ; ++message )
        {
            if ( self->state == message->continue_state )
                return message->stop_func(self, error);
        }
        return g_set_error(error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_UNEXPECTED_TOKEN, "Got '.' in an invalid state '%s'", _eventd_protocol_evp_states[self->state]);
    }

    /*
     * Handle dot message line
     */
    for ( message = _eventd_protocol_evp_dot_messages ; message->message != NULL ; ++message )
    {
        if ( self->state == message->continue_state )
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
                return g_set_error(error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_MALFORMED, "Message '%s' does take at least %" G_GSIZE_FORMAT " arguments, but got %" G_GSIZE_FORMAT, message->message, message->min_args, argc);
            }
        }
        else if ( message->min_args > 0 )
            return g_set_error(error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_MALFORMED, "Message '%s' does take at least %" G_GSIZE_FORMAT " arguments, but got none", message->message, message->min_args);

        gboolean valid = FALSE;
        for ( state = message->start_states ; *state != _EVENTD_PROTOCOL_EVP_STATE_SIZE ; ++state )
        {
            if ( self->state == *state )
                valid = TRUE;
        }
        if ( valid )
            message->start_func(self, (const gchar * const *) argv, error);
        else
            g_set_error(error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_UNEXPECTED_TOKEN, "Message '%s' in an invalid state '%s'", message->message, _eventd_protocol_evp_states[self->state]);
        return;
    }
}

gboolean
eventd_protocol_evp_parse(EventdProtocol *protocol, const gchar *buffer, GError **error)
{
    EventdProtocolEvp *self = (EventdProtocolEvp *) protocol;
    g_return_val_if_fail(self->state != _EVENTD_PROTOCOL_EVP_STATE_SIZE, FALSE);

    GError *_inner_error_ = NULL;

    gchar *l;
    l = g_utf8_strchr(buffer, -1, '\n');
    if ( l == NULL )
        _eventd_protocol_evp_parse_line(self, buffer, &_inner_error_);
    else
        g_set_error_literal(&_inner_error_, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_GARBAGE, "EvP parser only takes lines");

    if ( _inner_error_ == NULL )
        return TRUE;

    eventd_protocol_evp_parse_free(self);

    g_propagate_error(error, _inner_error_);
    return FALSE;
}

void
eventd_protocol_evp_parse_free(EventdProtocolEvp *self)
{
recheck:
    switch ( self->state )
    {
    case EVENTD_PROTOCOL_EVP_STATE_DOT_DATA:
        if ( self->data.hash != NULL )
            g_hash_table_unref(self->data.hash);
        self->data.hash = NULL;

        g_free(self->data.name);
        self->data.name = NULL;

        if ( self->data.value != NULL )
            g_string_free(self->data.value, TRUE);
        self->data.value = NULL;

        self->state = self->data.return_state;
    goto recheck;
    case EVENTD_PROTOCOL_EVP_STATE_DOT_EVENT:
        if ( self->data.hash != NULL )
            g_hash_table_unref(self->data.hash);
        self->data.hash = NULL;

        eventd_event_unref(self->event);
        self->event = NULL;
    break;
    case EVENTD_PROTOCOL_EVP_STATE_DOT_SUBSCRIBE:
        g_hash_table_unref(self->subscriptions);
        self->subscriptions = NULL;
    break;
    default:
    break;
    }

    self->state = _EVENTD_PROTOCOL_EVP_STATE_SIZE;
}
