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

#include <config.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <glib.h>
#include <glib-object.h>

#include <libeventd-event.h>
#include <libeventd-event-private.h>
#include <libeventd-protocol.h>

#include "protocol-evp-private.h"

#define DATA_SAMPLE ".DATA test-data\nSome data to put inside\nAnd here is a new line\nThat should be enough\n.\n"

static void
_eventd_protocol_generate_data(GString *str, GHashTable *data)
{
    if ( data == NULL )
        return;

    GHashTableIter iter;
    gchar *name, *value, *n;
    g_hash_table_iter_init(&iter, data);
    while ( g_hash_table_iter_next(&iter, (gpointer *) &name, (gpointer *) &value) )
    {
        n = g_utf8_strchr(value, -1, '\n');
        if ( n == NULL )
            g_string_append_printf(str, "DATA %s ", name);
        else
            g_string_append_printf(str, ".DATA %s\n", name);
        g_string_append(str, value);
        if ( n != NULL )
            g_string_append(str, "\n.");
        g_string_append_c(str, '\n');
    }

    g_hash_table_unref(data);
}

static gchar *
_eventd_protocol_evp_generate_event(EventdProtocol *protocol, EventdEvent *event, gchar *message)
{
    g_return_val_if_fail(EVENTD_IS_PROTOCOL_EVP(protocol), NULL);
    EventdProtocolEvp *self = EVENTD_PROTOCOL_EVP(protocol);

    eventd_protocol_evp_add_event(self, event);

    GHashTable *data;
    data = eventd_event_get_all_data(event);

    GList *answers;
    answers = eventd_event_get_answers(event);

    if ( ( data == NULL ) && ( answers == NULL ) )
        return g_strdup_printf("%s %s %s %s\n", message, eventd_event_get_uuid(event), eventd_event_get_category(event), eventd_event_get_name(event));

    gsize size;
    size = strlen(".EVENT 1b4e28ba-2fa1-11d2-883f-0016d3cca427 test-category test-name\n.\n") + ( ( data != NULL ) ? ( g_hash_table_size(data) * strlen(DATA_SAMPLE) ) : 0 ) + g_list_length(answers) * strlen("ANSWER test-answer");

    GString *str;
    str = g_string_sized_new(size);

    g_string_append_printf(str, ".%s %s %s %s\n", message, eventd_event_get_uuid(event), eventd_event_get_category(event), eventd_event_get_name(event));

    GList *answer;
    for ( answer = answers ; answer != NULL ; answer = g_list_next(answer) )
        g_string_append_printf(str, "ANSWER %s\n", (const gchar *) answer->data);

    _eventd_protocol_generate_data(str, data);

    g_string_append(str, ".\n");

    return g_string_free(str, FALSE);
}

gchar *
eventd_protocol_evp_generate_event(EventdProtocol *protocol, EventdEvent *event)
{
    return _eventd_protocol_evp_generate_event(protocol, event, "EVENT");
}

gchar *
eventd_protocol_evp_generate_relay(EventdProtocol *protocol, EventdEvent *event)
{
    return _eventd_protocol_evp_generate_event(protocol, event, "RELAY");
}

gchar *
eventd_protocol_evp_generate_updated(EventdProtocol *protocol, EventdEvent *event)
{
    g_return_val_if_fail(EVENTD_IS_PROTOCOL_EVP(protocol), NULL);
    EventdProtocolEvp *self = EVENTD_PROTOCOL_EVP(protocol);

    eventd_protocol_evp_add_event(self, event);

    GHashTable *data;
    data = eventd_event_get_all_data(event);

    GList *answers;
    answers = eventd_event_get_answers(event);

    gsize size;
    size = strlen(".UPDATED 1b4e28ba-2fa1-11d2-883f-0016d3cca427\n.\n") + ( ( data != NULL ) ? ( g_hash_table_size(data) * strlen(DATA_SAMPLE) ) : 0 ) + g_list_length(answers) * strlen("ANSWER test-answer");

    GString *str;
    str = g_string_sized_new(size);

    g_string_append_printf(str, ".UPDATED %s\n", eventd_event_get_uuid(event));

    GList *answer;
    for ( answer = answers ; answer != NULL ; answer = g_list_next(answer) )
        g_string_append_printf(str, "ANSWER %s\n", (const gchar *) answer->data);

    _eventd_protocol_generate_data(str, data);

    g_string_append(str, ".\n");

    return g_string_free(str, FALSE);
}

gchar *
eventd_protocol_evp_generate_answered(EventdProtocol *protocol, EventdEvent *event, const gchar *answer)
{
    g_return_val_if_fail(EVENTD_IS_PROTOCOL_EVP(protocol), NULL);
    EventdProtocolEvp *self = EVENTD_PROTOCOL_EVP(protocol);

    eventd_protocol_evp_add_event(self, event);

    GHashTable *data;
    data = eventd_event_get_all_answer_data(event);

    if ( data == NULL )
        return g_strdup_printf("ANSWERED %s %s\n", eventd_event_get_uuid(event), answer);

    gsize size;
    size = strlen(".ANSWERED 1b4e28ba-2fa1-11d2-883f-0016d3cca427 test-answer\n.\n") + g_hash_table_size(data) * strlen(DATA_SAMPLE);

    GString *str;
    str = g_string_sized_new(size);

    g_string_append_printf(str, ".ANSWERED %s %s\n", eventd_event_get_uuid(event), answer);

    _eventd_protocol_generate_data(str, data);

    g_string_append(str, ".\n");

    return g_string_free(str, FALSE);
}

gchar *
eventd_protocol_evp_generate_ended(EventdProtocol *protocol, EventdEvent *event, EventdEventEndReason reason)
{
    g_return_val_if_fail(EVENTD_IS_PROTOCOL_EVP(protocol), NULL);
    EventdProtocolEvp *self = EVENTD_PROTOCOL_EVP(protocol);

    eventd_protocol_evp_remove_event(self, event);

    const gchar *nick;
    nick = eventd_event_end_reason_get_value_nick(reason);

    return g_strdup_printf("ENDED %s %s\n", eventd_event_get_uuid(event), nick);
}


gchar *
eventd_protocol_evp_generate_passive(EventdProtocol *protocol)
{
    g_return_val_if_fail(EVENTD_IS_PROTOCOL_EVP(protocol), NULL);

    return g_strdup("PASSIVE\n");
}

gchar *
eventd_protocol_evp_generate_subscribe(EventdProtocol *protocol, GHashTable *categories)
{
    g_return_val_if_fail(EVENTD_IS_PROTOCOL_EVP(protocol), NULL);

    if ( categories == NULL )
        return g_strdup("SUBSCRIBE\n");

    gchar *message;

    guint length;
    GHashTableIter iter;
    gchar *category;
    gpointer dummy;
    length = g_hash_table_size(categories);
    g_hash_table_iter_init(&iter, categories);
    g_hash_table_iter_next(&iter, (gpointer *) &category, &dummy);

    if ( length == 1 )
    {
        message = g_strdup_printf("SUBSCRIBE %s\n", category);
        goto ret;
    }

    gsize size;
    GString *str;
    size = strlen(".SUBSCRIBE\n.\n") + ( strlen(category) + 1) * length;
    str = g_string_sized_new(size);

    g_string_append(str, ".SUBSCRIBE\n");
        g_string_append_c(g_string_append(str, category), '\n');
    while ( g_hash_table_iter_next(&iter, (gpointer *) &category, &dummy) )
        g_string_append_c(g_string_append(str, category), '\n');
    g_string_append(str, ".\n");

    message = g_string_free(str, FALSE);

ret:
    g_hash_table_unref(categories);
    return message;
}

gchar *
eventd_protocol_evp_generate_bye(EventdProtocol *protocol, const gchar *message)
{
    g_return_val_if_fail(EVENTD_IS_PROTOCOL_EVP(protocol), NULL);

    if ( message == NULL )
        return g_strdup("BYE\n");

    return g_strdup_printf("BYE %s\n", message);
}
