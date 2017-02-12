/*
 * libeventd-protocol - Main eventd library - Protocol manipulation
 *
 * Copyright © 2011-2017 Quentin "Sardem FF7" Glidic
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

#include "config.h"

#include <string.h>

#include <glib.h>
#include <glib-object.h>

#include "libeventd-event.h"
#include "libeventd-event-private.h"
#include "libeventd-protocol.h"

#include "protocol-evp-private.h"

static const gchar *_eventd_protocol_evp_states[_EVENTD_PROTOCOL_EVP_STATE_SIZE] = {
    [EVENTD_PROTOCOL_EVP_STATE_BASE]          = "base",
    [EVENTD_PROTOCOL_EVP_STATE_SUBSCRIBE]     = "subscribe",
    [EVENTD_PROTOCOL_EVP_STATE_BYE]           = "bye",
    [EVENTD_PROTOCOL_EVP_STATE_DOT_EVENT]     = "dot message EVENT",
    [EVENTD_PROTOCOL_EVP_STATE_DOT_SUBSCRIBE] = "dot message SUBSCRIBE",
    [EVENTD_PROTOCOL_EVP_STATE_IGNORING]      = "ignoring",
};

typedef void (*EventdProtocolTokenParseStartFunc)(EventdProtocol *self, const gchar * const *argv, GError **error);
typedef gboolean (*EventdProtocolTokenParseContinueFunc)(EventdProtocol *self, const gchar *line, GError **error);
typedef void (*EventdProtocolTokenParseStopFunc)(EventdProtocol *self, GError **error);
typedef struct {
    const gchar *message;
    gsize min_args;
    gsize max_args;
    EventdProtocolState start_states[_EVENTD_PROTOCOL_EVP_STATE_SIZE];
    EventdProtocolTokenParseStartFunc start_func;
    EventdProtocolState continue_state;
    EventdProtocolTokenParseContinueFunc continue_func;
    EventdProtocolTokenParseStopFunc stop_func;
} EventdProtocolTokens;


static inline void
eventd_protocol_call_event(EventdProtocol *self, EventdEvent *event)
{
    if ( self->callbacks->event != NULL )
        self->callbacks->event(self, event, self->user_data);
}

static inline void
eventd_protocol_call_subscribe(EventdProtocol *self, GHashTable *subscriptions)
{
    if ( self->callbacks->subscribe != NULL )
        self->callbacks->subscribe(self, subscriptions, self->user_data);
}

static inline void
eventd_protocol_call_bye(EventdProtocol *self, const gchar *message)
{
    if ( self->callbacks->bye != NULL )
        self->callbacks->bye(self, message, self->user_data);
}

static gboolean
_eventd_protocol_evp_parse_dot__continue_noeat(EventdProtocol *self, const gchar *line, GError **error)
{
    return FALSE;
}

/* dot messages catch-all */
static void
_eventd_protocol_evp_parse_dot_catchall_start(EventdProtocol *self, const gchar * const *argv, GError **error)
{
    ++self->catchall.level;
    self->state = EVENTD_PROTOCOL_EVP_STATE_IGNORING;
}

static gboolean
_eventd_protocol_evp_parse_dot_catchall_continue(EventdProtocol *self, const gchar *line, GError **error)
{
    if ( g_str_has_prefix(line, ".") && ( ! g_str_has_prefix(line, "..") ) )
        ++self->catchall.level;
    return TRUE;
}

static void
_eventd_protocol_evp_parse_dot_catchall_end(EventdProtocol *self, GError **error)
{
    if ( --self->catchall.level < 1 )
        self->state = self->base_state;
}

static EventdEvent *
_eventd_protocol_evp_parser_get_event(EventdProtocol *self, const gchar * const *argv, GError **error)
{
    EventdEvent *event;
    event = eventd_event_new_for_uuid_string(argv[0], argv[1], argv[2]);

    if ( event == NULL )
        g_set_error(error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_WRONG_UUID, "Error while parsing UUID %s", argv[0]);

    return event;
}

/* .EVENT */
static void
_eventd_protocol_evp_parse_dot_event_start(EventdProtocol *self, const gchar * const *argv, GError **error)
{
    self->event = _eventd_protocol_evp_parser_get_event(self, argv, error);

    if ( self->event == NULL )
        return _eventd_protocol_evp_parse_dot_catchall_start(self, argv, error);


    self->state = EVENTD_PROTOCOL_EVP_STATE_DOT_EVENT;
}

static void
_eventd_protocol_evp_parse_dot_event_end(EventdProtocol *self, GError **error)
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
_eventd_protocol_evp_parse_dot_subscribe_start(EventdProtocol *self, const gchar * const *argv, GError **error)
{
    self->subscriptions = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    self->state = EVENTD_PROTOCOL_EVP_STATE_DOT_SUBSCRIBE;
    self->base_state = EVENTD_PROTOCOL_EVP_STATE_SUBSCRIBE;
}

static gboolean
_eventd_protocol_evp_parse_dot_subscribe_continue(EventdProtocol *self, const gchar *line, GError **error)
{
    g_hash_table_add(self->subscriptions, g_strdup(line));
    return TRUE;
}

static void
_eventd_protocol_evp_parse_dot_subscribe_end(EventdProtocol *self, GError **error)
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
_eventd_protocol_evp_parse_data(EventdProtocol *self, const gchar * const *argv, GError **error)
{
    GError *_inner_error_ = NULL;
    GVariant *value;

    value = g_variant_parse(NULL, argv[1], NULL, NULL, &_inner_error_);
    if ( value == NULL )
    {
        g_set_error(error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_MALFORMED, "DATA content malformed: %s", _inner_error_->message);
        g_error_free(_inner_error_);
        return;
    }

    if ( self->data.hash == NULL )
        self->data.hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_variant_unref);
    g_hash_table_insert(self->data.hash, g_strdup(argv[0]), g_variant_ref_sink(value));
}

/* EVENT */
static void
_eventd_protocol_evp_parse_event(EventdProtocol *self, const gchar * const *argv, GError **error)
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
_eventd_protocol_evp_parse_subscribe(EventdProtocol *self, const gchar * const *argv, GError **error)
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
_eventd_protocol_evp_parse_bye(EventdProtocol *self, const gchar * const *argv, GError **error)
{
    eventd_protocol_call_bye((EventdProtocol *) self, ( argv == NULL ) ? NULL : argv[0]);

    self->base_state = EVENTD_PROTOCOL_EVP_STATE_BYE;
    self->state = self->base_state;
}

static const EventdProtocolTokens _eventd_protocol_evp_dot_messages[] = {
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
    { .message = NULL }
};

static const EventdProtocolTokens _eventd_protocol_evp_messages[] = {
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
    { .message = NULL }
};

static void
_eventd_protocol_evp_parse_line(EventdProtocol *self, const gchar *line, GError **error)
{
#ifdef EVENTD_DEBUG
    g_debug("[%s] Parse line: %.255s%s", _eventd_protocol_evp_states[self->state], line, ( strlen(line) > 255 ) ? " […]" : "");
#endif /* EVENTD_DEBUG */

    const EventdProtocolTokens *message;

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

    const EventdProtocolState *state;
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

/**
 * eventd_protocol_parse:
 * @protocol: an #EventdProtocol
 * @buffer: the buffer to parse (NUL-terminated)
 * @error: (out) (optional): return location for error or %NULL to ignore
 *
 * Parses @buffer for messages.
 *
 * Returns: %FALSE if there was an error, %TRUE otherwise
 */
EVENTD_EXPORT
gboolean
eventd_protocol_parse(EventdProtocol *protocol, gchar *buffer, GError **error)
{
    EventdProtocol *self = (EventdProtocol *) protocol;
    g_return_val_if_fail(self->state != _EVENTD_PROTOCOL_EVP_STATE_SIZE, FALSE);

    GError *_inner_error_ = NULL;

    gsize l;
    gchar *sl, *el;
    l = strlen(buffer);
    for ( sl = buffer, el = g_utf8_strchr(sl, l, '\n'); ( el != NULL ) && ( _inner_error_ == NULL ) ; sl = el + 1, el = g_utf8_strchr(sl, l - ( sl - buffer ), '\n') )
    {
        *el = '\0';
        _eventd_protocol_evp_parse_line(self, sl, &_inner_error_);
    }

    if ( ( _inner_error_ == NULL ) && ( sl < ( buffer + l ) ) )
        _eventd_protocol_evp_parse_line(self, sl, &_inner_error_);

    if ( _inner_error_ == NULL )
        return TRUE;

    eventd_protocol_evp_parse_free(self);

    g_propagate_error(error, _inner_error_);
    return FALSE;
}

void
eventd_protocol_evp_parse_free(EventdProtocol *self)
{
    switch ( self->state )
    {
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
