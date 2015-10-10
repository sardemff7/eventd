/*
 * libeventd-protocol-json - eventd JSON protocol library
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

#include <yajl/yajl_parse.h>

#include <libeventd-event.h>
#include <libeventd-event-private.h>
#include <libeventd-protocol.h>
#include <libeventd-protocol-json.h>

#include <nkutils-enum.h>

#include "protocol-private.h"
#include "protocol-json-private.h"

static const gchar *_eventd_protocol_json_message_types[_EVENTD_PROTOCOL_JSON_MESSAGE_TYPE_SIZE] = {
    [EVENTD_PROTOCOL_JSON_MESSAGE_TYPE_MISSING] = "missing",
    [EVENTD_PROTOCOL_JSON_MESSAGE_TYPE_EVENT]   = "event",
    [EVENTD_PROTOCOL_JSON_MESSAGE_TYPE_UNKNOWN] = "unknown",
};

static gint
_eventd_protocol_json_parse_null(gpointer user_data)
{
    EventdProtocolJson *self = user_data;

    g_set_error_literal(&self->priv->error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_UNEXPECTED_TOKEN, "Unexpected null");
    return 0;
}

static gint
_eventd_protocol_json_parse_boolean(gpointer user_data, gint val)
{
    EventdProtocolJson *self = user_data;

    g_set_error(&self->priv->error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_UNEXPECTED_TOKEN, "Unexpected boolean %s", val ? "true" : "false");
    return 0;
}

static gint
_eventd_protocol_json_parse_integer(gpointer user_data, long long val)
{
    EventdProtocolJson *self = user_data;

    g_set_error(&self->priv->error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_UNEXPECTED_TOKEN, "Unexpected integer %lld", val);
    return 0;
}

static gint
_eventd_protocol_json_parse_double(gpointer user_data, gdouble val)
{
    EventdProtocolJson *self = user_data;

    g_set_error(&self->priv->error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_UNEXPECTED_TOKEN, "Unexpected double %lf", val);
    return 0;
}

static gint
_eventd_protocol_json_parse_number(gpointer user_data, const char *numberVal, gsize numberLen)
{
    EventdProtocolJson *self = user_data;

    g_set_error_literal(&self->priv->error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_UNEXPECTED_TOKEN, "Unexpected number");
    return 0;
}

static gint
_eventd_protocol_json_parse_string(gpointer user_data, const guchar *str_, gsize len)
{
    EventdProtocolJson *self = user_data;
    gchar *str;
    str = g_strndup((const gchar *) str_, len);

    switch ( self->priv->message.state )
    {
    case EVENTD_PROTOCOL_JSON_STATE_DATA:
        g_hash_table_insert(self->priv->message.data, self->priv->message.data_name, str);
        self->priv->message.data_name = NULL;
        return 1;
    case EVENTD_PROTOCOL_JSON_STATE_TYPE:
    {
        guint64 type = EVENTD_PROTOCOL_JSON_MESSAGE_TYPE_UNKNOWN;
        nk_enum_parse(str, _eventd_protocol_json_message_types, _EVENTD_PROTOCOL_JSON_MESSAGE_TYPE_SIZE, TRUE, &type);
        self->priv->message.type = type;
        g_free(str);
    }
    break;
    case EVENTD_PROTOCOL_JSON_STATE_UUID:
    {
        uuid_t uuid;
        if ( uuid_parse(str, uuid) < 0 )
        {
            g_set_error(&self->priv->error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_WRONG_UUID, "Error while parsing UUID '%s'", str);
            g_free(str);
            return 0;
        }
        uuid_copy(self->priv->message.uuid, uuid);
        g_free(str);
    }
    break;
    case EVENTD_PROTOCOL_JSON_STATE_CATEGORY:
        self->priv->message.category = str;
    break;
    case EVENTD_PROTOCOL_JSON_STATE_NAME:
        self->priv->message.name = str;
    break;
    case EVENTD_PROTOCOL_JSON_STATE_ANSWERS:
        self->priv->message.answers = g_list_prepend(self->priv->message.answers, str);
    break;
    default:
        g_set_error(&self->priv->error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_UNEXPECTED_TOKEN, "Unexpected string '%s'", str);
        g_free(str);
        return 0;
    }
    self->priv->message.state = EVENTD_PROTOCOL_JSON_STATE_MESSAGE;
    return 1;
}

static gint
_eventd_protocol_json_parse_start_map(gpointer user_data)
{
    EventdProtocolJson *self = user_data;

    switch ( self->priv->message.state )
    {
    case EVENTD_PROTOCOL_JSON_STATE_BASE:
        self->priv->message.state = EVENTD_PROTOCOL_JSON_STATE_MESSAGE;
        return 1;
    case EVENTD_PROTOCOL_JSON_STATE_DATA:
        self->priv->message.data = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        return 1;
    default:
    break;
    }

    g_set_error_literal(&self->priv->error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_UNEXPECTED_TOKEN, "Unexpected map");
    return 0;
}

static gint
_eventd_protocol_json_parse_map_key(gpointer user_data, const guchar *key_, gsize len)
{
    EventdProtocolJson *self = user_data;
    const gchar *key = (const gchar *) key_;

    switch ( self->priv->message.state )
    {
    case EVENTD_PROTOCOL_JSON_STATE_DATA:
        self->priv->message.data_name = g_strndup(key, len);
        return 1;
    case EVENTD_PROTOCOL_JSON_STATE_MESSAGE:
        if ( g_ascii_strncasecmp(key, "data", len) == 0 )
        {
            if ( self->priv->message.data != NULL )
            {
                g_set_error_literal(&self->priv->error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_MALFORMED, "Got at least two data objects");
                break;
            }
            self->priv->message.state = EVENTD_PROTOCOL_JSON_STATE_DATA;
        }
        else if ( g_ascii_strncasecmp(key, "message-type", len) == 0 )
        {
            if ( self->priv->message.type != EVENTD_PROTOCOL_JSON_MESSAGE_TYPE_MISSING )
            {
                g_set_error_literal(&self->priv->error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_MALFORMED, "Got at least two message-type");
                break;
            }
            self->priv->message.state = EVENTD_PROTOCOL_JSON_STATE_TYPE;
        }
        else if ( g_ascii_strncasecmp(key, "uuid", len) == 0 )
        {
            if ( ! uuid_is_null(self->priv->message.uuid) )
            {
                g_set_error_literal(&self->priv->error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_MALFORMED, "Got at least two UUIDs");
                break;
            }
            self->priv->message.state = EVENTD_PROTOCOL_JSON_STATE_UUID;
        }
        else if ( g_ascii_strncasecmp(key, "category", len) == 0 )
        {
            if ( self->priv->message.category != NULL )
            {
                g_set_error_literal(&self->priv->error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_MALFORMED, "Got at least two categories");
                break;
            }
            self->priv->message.state = EVENTD_PROTOCOL_JSON_STATE_CATEGORY;
        }
        else if ( g_ascii_strncasecmp(key, "name", len) == 0 )
        {
            if ( self->priv->message.name != NULL )
            {
                g_set_error_literal(&self->priv->error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_MALFORMED, "Got at least two names");
                break;
            }
            self->priv->message.state = EVENTD_PROTOCOL_JSON_STATE_NAME;
        }
        else if ( g_ascii_strncasecmp(key, "answers", len) == 0 )
        {
            if ( self->priv->message.answers != NULL )
            {
                g_set_error_literal(&self->priv->error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_MALFORMED, "Got at least two answers lists");
                break;
            }
            self->priv->message.state = EVENTD_PROTOCOL_JSON_STATE_ANSWERS;
        }
        return 1;
    default:
    break;
    }

    g_set_error_literal(&self->priv->error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_UNEXPECTED_TOKEN, "Unexpected map key");
    return 0;
}

static gint
_eventd_protocol_json_parse_end_map(gpointer user_data)
{
    EventdProtocolJson *self = user_data;

    switch ( self->priv->message.state )
    {
    case EVENTD_PROTOCOL_JSON_STATE_MESSAGE:
        self->priv->message.state = EVENTD_PROTOCOL_JSON_STATE_BASE;
        return 1;
    case EVENTD_PROTOCOL_JSON_STATE_DATA:
        self->priv->message.state = EVENTD_PROTOCOL_JSON_STATE_MESSAGE;
        return 1;
    default:
    break;
    }

    g_set_error_literal(&self->priv->error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_UNEXPECTED_TOKEN, "Unexpected map end");
    return 0;
}

static gint
_eventd_protocol_json_parse_start_array(gpointer user_data)
{
    EventdProtocolJson *self = user_data;

    switch ( self->priv->message.state )
    {
    case EVENTD_PROTOCOL_JSON_STATE_ANSWERS:
        return 1;
    default:
    break;
    }

    g_set_error_literal(&self->priv->error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_UNEXPECTED_TOKEN, "Unexpected array");
    return 0;
}

static gint
_eventd_protocol_json_parse_end_array(gpointer user_data)
{
    EventdProtocolJson *self = user_data;

    switch ( self->priv->message.state )
    {
    case EVENTD_PROTOCOL_JSON_STATE_ANSWERS:
        self->priv->message.state = EVENTD_PROTOCOL_JSON_STATE_MESSAGE;
        return 1;
    default:
    break;
    }

    g_set_error_literal(&self->priv->error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_UNEXPECTED_TOKEN, "Unexpected array end");
    return 0;
}

static const yajl_callbacks _eventd_protocol_json_parse_callbacks = {
    .yajl_null        = _eventd_protocol_json_parse_null,
    .yajl_boolean     = _eventd_protocol_json_parse_boolean,
    .yajl_integer     = _eventd_protocol_json_parse_integer,
    .yajl_double      = _eventd_protocol_json_parse_double,
    .yajl_number      = _eventd_protocol_json_parse_number,
    .yajl_string      = _eventd_protocol_json_parse_string,
    .yajl_start_map   = _eventd_protocol_json_parse_start_map,
    .yajl_map_key     = _eventd_protocol_json_parse_map_key,
    .yajl_end_map     = _eventd_protocol_json_parse_end_map,
    .yajl_start_array = _eventd_protocol_json_parse_start_array,
    .yajl_end_array   = _eventd_protocol_json_parse_end_array,
};

static gboolean
_eventd_protocol_json_check_message(EventdProtocolJson *self, GError **error)
{
    switch ( self->priv->message.type )
    {
    case EVENTD_PROTOCOL_JSON_MESSAGE_TYPE_EVENT:
    {
        EventdEvent *event;
        event = eventd_event_new_for_uuid(self->priv->message.uuid, self->priv->message.category, self->priv->message.name);
        if ( self->priv->message.data != NULL )
            eventd_event_set_all_data(event, self->priv->message.data);
        g_signal_emit(self, eventd_protocol_signals[SIGNAL_EVENT], 0, event);
        //g_signal_emit_by_name(self, "event", event);
        g_object_unref(event);

        uuid_clear(self->priv->message.uuid);
        g_free(self->priv->message.category);
        g_free(self->priv->message.name);
        self->priv->message.category = NULL;
        self->priv->message.name = NULL;
        self->priv->message.data = NULL;
    }
    case EVENTD_PROTOCOL_JSON_MESSAGE_TYPE_UNKNOWN:
        return TRUE;
    case EVENTD_PROTOCOL_JSON_MESSAGE_TYPE_MISSING:
        g_set_error_literal(error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_MALFORMED, "Message missing type");
    break;
    case _EVENTD_PROTOCOL_JSON_MESSAGE_TYPE_SIZE:
        g_return_val_if_reached(FALSE);
    }
    return FALSE;
}

gboolean
eventd_protocol_json_parse(EventdProtocol *protocol, gchar **buffer, GError **error)
{
    g_return_val_if_fail(EVENTD_IS_PROTOCOL_JSON(protocol), FALSE);
    EventdProtocolJson *self = EVENTD_PROTOCOL_JSON(protocol);

    const guchar *json = (const guchar *) *buffer;
    gsize json_length = strlen(*buffer), consumed;
    yajl_handle hand;
    yajl_status status;
    gboolean ret = FALSE;

    hand = yajl_alloc(&_eventd_protocol_json_parse_callbacks, &eventd_protocol_json_alloc_funcs, self);
    status = yajl_parse(hand, json, json_length);
    consumed = yajl_get_bytes_consumed(hand);
    if ( status == yajl_status_ok )
    {
        status = yajl_complete_parse(hand);
        consumed += yajl_get_bytes_consumed(hand);
    }

    switch ( status )
    {
    case yajl_status_ok:
    {
        if ( ( self->priv->message.state == EVENTD_PROTOCOL_JSON_STATE_BASE ) && ( consumed == ( json_length + 1 ) ) )
            ret = _eventd_protocol_json_check_message(self, error);
        else
            g_set_error_literal(error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_MALFORMED, "Malformed JSON message");
    }
    break;
    case yajl_status_client_canceled:
        g_propagate_error(error, self->priv->error);
        self->priv->error = NULL;
    break;
    case yajl_status_error:
    {
        guchar *error_message;
        error_message = yajl_get_error(hand, TRUE, json, json_length);
        g_set_error(error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_MALFORMED, "Malformed JSON message: %s", (const gchar *) error_message);
        yajl_free_error(hand, error_message);
    }
    break;
    }

    eventd_protocol_json_parse_free(self);
    yajl_free(hand);
    g_free(*buffer);
    return ret;
}

void
eventd_protocol_json_parse_free(EventdProtocolJson *self)
{
}
