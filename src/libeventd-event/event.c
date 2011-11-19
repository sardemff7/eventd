/*
 * libeventd-event - Library to manipulate eventd events
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
#include <glib-object.h>

#include <libeventd-event.h>

#define EVENTD_TYPE_EVENT (_eventd_event_get_type())
#define EVENTD_TYPE_EVENT_END_REASON (_eventd_event_end_reason_get_type())

G_DEFINE_TYPE(EventdEvent, _eventd_event, G_TYPE_OBJECT)

GType
_eventd_event_end_reason_get_type()
{
    static volatile gsize g_define_type_id__volatile = 0;

    if ( g_once_init_enter(&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            { EVENTD_EVENT_END_REASON_NONE,           "EVENTD_EVENT_END_REASON_NONE",           "none" },
            { EVENTD_EVENT_END_REASON_TIMEOUT,        "EVENTD_EVENT_END_REASON_TIMEOUT",        "timeout" },
            { EVENTD_EVENT_END_REASON_USER_DISMISS,   "EVENTD_EVENT_END_REASON_USER_DISMISS",   "user-dismiss" },
            { EVENTD_EVENT_END_REASON_CLIENT_DISMISS, "EVENTD_EVENT_END_REASON_CLIENT_DISMISS", "client-dismiss" },
            { EVENTD_EVENT_END_REASON_RESERVED,       "EVENTD_EVENT_END_REASON_RESERVED",       "reserved" },
            { 0, NULL, NULL }
        };
        GType g_define_type_id = g_enum_register_static(g_intern_static_string("EventdEventEndReason"), values);
        g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}

struct _EventdEventPrivate {
    gchar *type;
    GHashTable *data;
    GHashTable *pong_data;

    guint64 ref_count;
};


static void _eventd_event_finalize(GObject *object);

static void
_eventd_event_class_init(EventdEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    _eventd_event_parent_class = g_type_class_peek_parent(klass);

    object_class->finalize = _eventd_event_finalize;
}

EventdEvent *
eventd_event_new(const gchar *type)
{
    EventdEvent *event;

    event = g_object_new(EVENTD_TYPE_EVENT, NULL);

    event->priv->type = g_strdup(type);

    return event;
}

static void
_eventd_event_init(EventdEvent *event)
{
    event->priv = g_new0(EventdEventPrivate, 1);
}

static void
_eventd_event_finalize(GObject *object)
{
    EventdEvent *event = (EventdEvent *)object;
    EventdEventPrivate *priv = event->priv;

    if ( priv->pong_data != NULL )
        g_hash_table_unref(priv->pong_data);
    if ( priv->data != NULL )
        g_hash_table_unref(priv->data);
    g_free(priv->type);
    g_free(priv);

    G_OBJECT_CLASS(_eventd_event_parent_class)->finalize(object);
}

void
eventd_event_add_data(EventdEvent *event, gchar *name, gchar *content)
{
    if ( event->priv->data == NULL )
        event->priv->data = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(event->priv->data, name, content);
}

void
eventd_event_add_pong_data(EventdEvent *event, gchar *name, gchar *content)
{
    if ( event->priv->pong_data == NULL )
        event->priv->pong_data = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(event->priv->pong_data, name, content);
}


const gchar *
eventd_event_get_type(EventdEvent *event)
{
    return event->priv->type;
}

GHashTable *
eventd_event_get_data(EventdEvent *event)
{
    return event->priv->data;
}

GHashTable *
eventd_event_get_pong_data(EventdEvent *event)
{
    return event->priv->pong_data;
}

