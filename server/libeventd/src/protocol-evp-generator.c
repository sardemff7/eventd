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

#define DATA_SAMPLE ".DATA test-data\nSome data to put inside\nAnd here is a new line\nThat should be enough\n.\n"

static void
_eventd_protocol_generate_data(GString *str, GHashTable *data)
{
    if ( data == NULL )
        return;

    GHashTableIter iter;
    gchar *name;
    GVariant *value;
    g_hash_table_iter_init(&iter, data);
    while ( g_hash_table_iter_next(&iter, (gpointer *) &name, (gpointer *) &value) )
    {
        g_string_append_printf(str, "DATA %s ", name);
        g_variant_print_string(value, str, ! g_variant_is_of_type(value, G_VARIANT_TYPE_STRING));
        g_string_append_c(str, '\n');
    }

    g_hash_table_unref(data);
}

/**
 * eventd_protocol_generate_event:
 * @protocol: an #EventdProtocol
 * @event: the #EventdEVent to generate a message for
 *
 * Generates an EVENT message.
 *
 * Returns: (transfer full): the message
 */
EVENTD_EXPORT
gchar *
eventd_protocol_generate_event(EventdProtocol *protocol, EventdEvent *event)
{
    GHashTable *data;
    data = eventd_event_get_all_data(event);

    if ( data == NULL )
        return g_strdup_printf("EVENT %s %s %s\n", eventd_event_get_uuid(event), eventd_event_get_category(event), eventd_event_get_name(event));

    gsize size;
    size = strlen(".EVENT 1b4e28ba-2fa1-11d2-883f-0016d3cca427 test-category test-name\n.\n") + ( g_hash_table_size(data) * strlen(DATA_SAMPLE) );

    GString *str;
    str = g_string_sized_new(size);

    g_string_append_printf(str, ".EVENT %s %s %s\n", eventd_event_get_uuid(event), eventd_event_get_category(event), eventd_event_get_name(event));

    _eventd_protocol_generate_data(str, data);

    g_string_append(str, ".\n");

    return g_string_free(str, FALSE);
}

/**
 * eventd_protocol_generate_subscribe:
 * @protocol: an #EventdProtocol
 * @categories: (element-type utf8 utf8) (nullable): the categories of events you want to subscribe to as a set (key == value)
 *
 * Generates a SUBSCRIBE message.
 *
 * Returns: (transfer full): the message
 */
EVENTD_EXPORT
gchar *
eventd_protocol_generate_subscribe(EventdProtocol *protocol, GHashTable *categories)
{
    if ( categories == NULL )
        return g_strdup("SUBSCRIBE\n");

    guint length;
    GHashTableIter iter;
    gchar *category;
    length = g_hash_table_size(categories);
    g_hash_table_iter_init(&iter, categories);
    if ( ! g_hash_table_iter_next(&iter, (gpointer *) &category, NULL) )
        return g_strdup("SUBSCRIBE\n");

    if ( length == 1 )
        return g_strdup_printf("SUBSCRIBE %s\n", category);

    gsize size;
    GString *str;
    size = strlen(".SUBSCRIBE\n.\n") + ( strlen(category) + 1) * length;
    str = g_string_sized_new(size);

    g_string_append(str, ".SUBSCRIBE\n");
    do
        g_string_append_c(g_string_append(str, category), '\n');
    while ( g_hash_table_iter_next(&iter, (gpointer *) &category, NULL) );
    g_string_append(str, ".\n");

    return g_string_free(str, FALSE);
}

/**
 * eventd_protocol_generate_ping:
 * @protocol: an #EventdProtocol
 *
 * Generates a PING message.
 *
 * Returns: (transfer full): the message
 */
EVENTD_EXPORT
gchar *
eventd_protocol_generate_ping(EventdProtocol *protocol)
{
    return g_strdup("PING\n");
}

/**
 * eventd_protocol_generate_bye:
 * @protocol: an #EventdProtocol
 * @message: (nullable): an optional message to send
 *
 * Generates a BYE message.
 *
 * Returns: (transfer full): the message
 */
EVENTD_EXPORT
gchar *
eventd_protocol_generate_bye(EventdProtocol *protocol, const gchar *message)
{
    if ( message == NULL )
        return g_strdup("BYE\n");

    return g_strdup_printf("BYE %s\n", message);
}
