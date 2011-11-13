/*
 * libeventd - Internal helper
 *
 * Copyright © 2011 Quentin "Sardem FF7" Glidic
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

#include <glib.h>

#include <libeventd-event.h>

struct _EventdEvent {
    gchar *type;
    GHashTable *data;

    guint64 ref_count;
};

EventdEvent *
libeventd_event_new(const gchar *type)
{
    EventdEvent *event;

    event = g_new0(EventdEvent, 1);
    event->ref_count = 1;

    event->type = g_strdup(type);
    event->data = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    return event;
}

static void
_libeventd_event_finalize(EventdEvent *event)
{
    g_hash_table_unref(event->data);
    g_free(event->type);
    g_free(event);
}

void
libeventd_event_ref(EventdEvent *event)
{
    if ( event == NULL )
        return;

    ++event->ref_count;
}

void
libeventd_event_unref(EventdEvent *event)
{
    if ( event == NULL )
        return;

    if ( --event->ref_count < 1 )
        _libeventd_event_finalize(event);
}

void
libeventd_event_add_data(EventdEvent *event, gchar *name, gchar *content)
{
    g_hash_table_insert(event->data, name, content);
}

const gchar *
libeventd_event_get_type(EventdEvent *event)
{
    return event->type;
}

GHashTable *
libeventd_event_get_data(EventdEvent *event)
{
    return event->data;
}
