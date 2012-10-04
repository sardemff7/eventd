/*
 * eventd - Small daemon to act on remote or local events
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

#include <glib.h>
#include <glib-compat.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <libeventd-event.h>

#include "types.h"

#include "plugins.h"
#include "config.h"

#include "queue.h"

struct _EventdQueue {
    EventdConfig *config;
    GAsyncQueue *queue;
    GThread *thread;
    GSList *current;
    guint64 current_count;
};

typedef struct {
    EventdQueue *queue;
    const gchar *config_id;
    EventdEvent *event;
    guint timeout_id;
} EventdQueueEvent;


static gint64
_eventd_queue_event_set_timeout(EventdQueue *queue, const gchar *config_id, EventdEvent *event)
{
    gint64 timeout;

    timeout = eventd_event_get_timeout(event);
    if ( timeout < 0 )
        timeout = eventd_config_event_get_timeout(queue->config, config_id);
    eventd_event_set_timeout(event, timeout);

    return timeout;
}

static gboolean
_eventd_queue_event_timeout(gpointer user_data)
{
    EventdQueueEvent *event = user_data;

    event->timeout_id = 0;

    eventd_event_end(event->event, EVENTD_EVENT_END_REASON_TIMEOUT);

    return FALSE;
}

static void
_eventd_queue_event_updated(GObject *object, gpointer user_data)
{
    EventdQueueEvent *event = user_data;
    gint64 timeout;

    g_source_remove(event->timeout_id);

    timeout = _eventd_queue_event_set_timeout(event->queue, event->config_id, event->event);

    if ( timeout > 0 )
        event->timeout_id = g_timeout_add(timeout, _eventd_queue_event_timeout, event);
}

static void
_eventd_queue_event_ended(GObject *object, gint reason, gpointer user_data)
{
    EventdQueueEvent *event = user_data;
    EventdQueue *queue = event->queue;
    guint64 stack = eventd_config_get_stack(queue->config);

    if ( event->timeout_id > 0 )
        g_source_remove(event->timeout_id);

    queue->current = g_slist_remove(queue->current, event);
    if ( ( stack > 0 ) && ( --queue->current_count < stack ) )
        g_async_queue_unlock(queue->queue);

    g_object_unref(event->event);
    g_free(event);
}

static gpointer
_eventd_queue_source_dispatch(gpointer user_data)
{
    EventdQueue *queue = user_data;
    EventdQueueEvent *event;
    gint timeout;

    while ( ( event = g_async_queue_pop(queue->queue) ) && ( event->event != NULL ) )
    {
        guint64 stack = eventd_config_get_stack(queue->config);
        if ( ( stack > 0 ) && ( ++queue->current_count >= stack ) )
            g_async_queue_lock(queue->queue);
        queue->current = g_slist_prepend(queue->current, event);

        eventd_plugins_event_action_all(event->config_id, event->event);

        g_signal_connect(event->event, "updated", G_CALLBACK(_eventd_queue_event_updated), event);
        g_signal_connect(event->event, "ended", G_CALLBACK(_eventd_queue_event_ended), event);
        timeout = _eventd_queue_event_set_timeout(queue, event->config_id, event->event);
        if ( timeout > 0 )
            event->timeout_id = g_timeout_add(timeout, _eventd_queue_event_timeout, event);
    }
    g_free(event);

    return NULL;
}

EventdQueue *
eventd_queue_new(EventdConfig *config)
{
    EventdQueue *queue;

    queue = g_new0(EventdQueue, 1);

    queue->config = config;

    queue->queue = g_async_queue_new();

    return queue;
}

void
eventd_queue_start(EventdQueue *queue)
{
    queue->thread = g_thread_new("Event queue", _eventd_queue_source_dispatch, queue);
}

void
eventd_queue_stop(EventdQueue *queue)
{
    if ( queue->current != NULL )
    {
        GSList *current;
        for ( current = queue->current ; current != NULL ; current = g_slist_next(current) )
        {
            EventdQueueEvent *event = current->data;
            eventd_event_end(event->event, EVENTD_EVENT_END_REASON_RESERVED);
        }
    }

    g_async_queue_push(queue->queue, g_new0(EventdQueueEvent, 1));

    g_thread_join(queue->thread);
}

void
eventd_queue_free(EventdQueue *queue)
{
    g_async_queue_unref(queue->queue);

    g_free(queue);
}

void
eventd_queue_push(EventdQueue *queue, const gchar *config_id, EventdEvent *event)
{
    EventdQueueEvent *queue_event;

    queue_event = g_new0(EventdQueueEvent, 1);
    queue_event->queue = queue;
    queue_event->config_id = config_id;
    queue_event->event = g_object_ref(event);

    g_async_queue_push_unlocked(queue->queue, queue_event);
}
