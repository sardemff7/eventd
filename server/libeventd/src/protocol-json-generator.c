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

#include <config.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <glib.h>
#include <glib-object.h>

#include <yajl/yajl_gen.h>

#include <libeventd-event.h>
#include <libeventd-event-private.h>
#include <libeventd-protocol.h>
#include <libeventd-protocol-json.h>

#include "protocol-json-private.h"

#define DATA_SAMPLE ".DATA test-data\nSome data to put inside\nAnd here is a new line\nThat should be enough\n.\n"

#define _yajl_gen_string(g, s) yajl_gen_string(g, (const guchar *) (s), strlen(s))

static yajl_gen
_eventd_protocol_json_message_open(const gchar *type, EventdEvent *event)
{
    yajl_gen gen;
    gen = yajl_gen_alloc(&eventd_protocol_json_alloc_funcs);

    yajl_gen_map_open(gen);

    _yajl_gen_string(gen, "message-type");
    _yajl_gen_string(gen, type);

    if ( event != NULL )
    {
        _yajl_gen_string(gen, "uuid");
        _yajl_gen_string(gen, eventd_event_get_uuid(event));
    }

    return gen;
}

static gchar *
_eventd_protocol_json_message_close(yajl_gen gen)
{
    yajl_gen_map_close(gen);

    const guchar *buf;
    gsize len;
    gchar *message;

    yajl_gen_get_buf(gen, &buf, &len);
    message = g_strndup((const gchar *) buf, len);

    yajl_gen_free(gen);

    return message;
}

static void
_eventd_protocol_json_generate_data(yajl_gen gen, GHashTable *data)
{
    if ( data == NULL )
        return;

    _yajl_gen_string(gen, "data");
    yajl_gen_map_open(gen);

    GHashTableIter iter;
    gchar *name, *value;
    g_hash_table_iter_init(&iter, data);
    while ( g_hash_table_iter_next(&iter, (gpointer *) &name, (gpointer *) &value) )
    {
        _yajl_gen_string(gen, name);
        _yajl_gen_string(gen, value);
    }

    yajl_gen_map_close(gen);

    g_hash_table_unref(data);
}

static gchar *
_eventd_protocol_json_generate_event(EventdProtocol *protocol, EventdEvent *event, const gchar *message)
{
    g_return_val_if_fail(EVENTD_IS_PROTOCOL_JSON(protocol), NULL);
    EventdProtocolJson *self = EVENTD_PROTOCOL_JSON(protocol);

    eventd_protocol_json_add_event(self, event);

    yajl_gen gen;
    gen = _eventd_protocol_json_message_open(message, event);

    _yajl_gen_string(gen, "category");
    _yajl_gen_string(gen, eventd_event_get_category(event));
    _yajl_gen_string(gen, "name");
    _yajl_gen_string(gen, eventd_event_get_name(event));

    GHashTable *data;
    data = eventd_event_get_all_data(event);

    GList *answers;
    answers = eventd_event_get_answers(event);

    if ( answers != NULL )
    {
        _yajl_gen_string(gen, "answers");
        yajl_gen_array_open(gen);
        GList *answer;
        for ( answer = answers ; answer != NULL ; answer = g_list_next(answer) )
            yajl_gen_string(gen, answer->data, strlen(answer->data));
        yajl_gen_array_close(gen);
    }

    _eventd_protocol_json_generate_data(gen, data);

    return _eventd_protocol_json_message_close(gen);
}

gchar *
eventd_protocol_json_generate_event(EventdProtocol *protocol, EventdEvent *event)
{
    return _eventd_protocol_json_generate_event(protocol, event, "event");
}

gchar *
eventd_protocol_json_generate_relay(EventdProtocol *protocol, EventdEvent *event)
{
    return _eventd_protocol_json_generate_event(protocol, event, "relay");
}

gchar *
eventd_protocol_json_generate_updated(EventdProtocol *protocol, EventdEvent *event)
{
    g_return_val_if_fail(EVENTD_IS_PROTOCOL_JSON(protocol), NULL);
    EventdProtocolJson *self = EVENTD_PROTOCOL_JSON(protocol);

    eventd_protocol_json_add_event(self, event);

    yajl_gen gen;
    gen = _eventd_protocol_json_message_open("updated", event);

    GHashTable *data;
    data = eventd_event_get_all_data(event);

    GList *answers;
    answers = eventd_event_get_answers(event);

    if ( answers != NULL )
    {
        _yajl_gen_string(gen, "answers");
        yajl_gen_array_open(gen);
        GList *answer;
        for ( answer = answers ; answer != NULL ; answer = g_list_next(answer) )
            yajl_gen_string(gen, answer->data, strlen(answer->data));
        yajl_gen_array_close(gen);
    }

    _eventd_protocol_json_generate_data(gen, data);

    return _eventd_protocol_json_message_close(gen);
}

gchar *
eventd_protocol_json_generate_answered(EventdProtocol *protocol, EventdEvent *event, const gchar *answer)
{
    g_return_val_if_fail(EVENTD_IS_PROTOCOL_JSON(protocol), NULL);
    EventdProtocolJson *self = EVENTD_PROTOCOL_JSON(protocol);

    eventd_protocol_json_add_event(self, event);

    yajl_gen gen;
    gen = _eventd_protocol_json_message_open("answered", event);

    _yajl_gen_string(gen, "answer");
    _yajl_gen_string(gen, answer);

    _eventd_protocol_json_generate_data(gen, eventd_event_get_all_answer_data(event));

    return _eventd_protocol_json_message_close(gen);
}

gchar *
eventd_protocol_json_generate_ended(EventdProtocol *protocol, EventdEvent *event, EventdEventEndReason reason)
{
    g_return_val_if_fail(EVENTD_IS_PROTOCOL_JSON(protocol), NULL);
    EventdProtocolJson *self = EVENTD_PROTOCOL_JSON(protocol);

    eventd_protocol_json_remove_event(self, event);

    yajl_gen gen;
    gen = _eventd_protocol_json_message_open("ended", event);

    _yajl_gen_string(gen, "reason");
    _yajl_gen_string(gen, eventd_event_end_reason_get_value_nick(reason));

    return _eventd_protocol_json_message_close(gen);
}


gchar *
eventd_protocol_json_generate_passive(EventdProtocol *protocol)
{
    g_return_val_if_fail(EVENTD_IS_PROTOCOL_JSON(protocol), NULL);

    return NULL;
}

gchar *
eventd_protocol_json_generate_subscribe(EventdProtocol *protocol, GHashTable *categories)
{
    g_return_val_if_fail(EVENTD_IS_PROTOCOL_JSON(protocol), NULL);

    yajl_gen gen;
    gen = _eventd_protocol_json_message_open("subscribe", NULL);

    if ( categories != NULL )
    {
        yajl_gen_array_open(gen);

        GHashTableIter iter;
        gchar *category;
        gpointer dummy;
        g_hash_table_iter_init(&iter, categories);
        while ( g_hash_table_iter_next(&iter, (gpointer *) &category, &dummy) )
            _yajl_gen_string(gen, category);

        yajl_gen_array_close(gen);
        g_hash_table_unref(categories);
    }

    return _eventd_protocol_json_message_close(gen);
}

gchar *
eventd_protocol_json_generate_bye(EventdProtocol *protocol, const gchar *message)
{
    g_return_val_if_fail(EVENTD_IS_PROTOCOL_JSON(protocol), NULL);

    return NULL;
}
