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

#include "queue.h"

typedef struct {
    EventdEvent *event;
    guint timeout_id;
} EventdQueueEvent;

struct _EventdQueue {
    EventdService *service;
    GAsyncQueue *queue;
    GThread *thread;
    EventdQueueEvent *current;
    guint32 count;
};

guint32
eventd_queue_get_next_event_id(EventdQueue *queue)
{
    return ++queue->count;
}

static void
_eventd_queue_event_ended(GObject *object, gint reason, gpointer user_data)
{
    EventdQueue *queue = user_data;
    EventdQueueEvent *event = queue->current;

    if ( event->timeout_id > 0 )
        g_source_remove(event->timeout_id);

    queue->current = NULL;
    g_async_queue_unlock(queue->queue);

    g_object_unref(event->event);
    g_free(event);
}

static gboolean
_eventd_queue_event_timeout(gpointer user_data)
{
    EventdQueueEvent *event = user_data;

    event->timeout_id = 0;

    eventd_event_end(event->event, EVENTD_EVENT_END_REASON_TIMEOUT);

    return FALSE;
}

static gpointer
_eventd_queue_source_dispatch(gpointer user_data)
{
    EventdQueue *queue = user_data;
    EventdQueueEvent *event;
    gint timeout;

    while ( ( event = g_async_queue_pop(queue->queue) ) && ( event->event != NULL ) )
    {
        g_async_queue_lock(queue->queue);
        queue->current = event;

        eventd_plugins_event_action_all(event->event);

        g_signal_connect(event->event, "ended", G_CALLBACK(_eventd_queue_event_ended), queue);
        timeout = eventd_event_get_timeout(event->event);
        if ( timeout < 0 )
            timeout = 3000;
        if ( timeout > 0 )
            event->timeout_id = g_timeout_add(timeout, _eventd_queue_event_timeout, event);
    }
    g_free(event);

    return NULL;
}

EventdQueue *
eventd_queue_new(EventdService *service)
{
    EventdQueue *queue;

    queue = g_new0(EventdQueue, 1);

    queue->service = service;

    queue->queue = g_async_queue_new();

    queue->thread = g_thread_new("Event queue", _eventd_queue_source_dispatch, queue);

    return queue;
}

void
eventd_queue_free(EventdQueue *queue)
{
    if ( queue->current != NULL )
        eventd_event_end(queue->current->event, EVENTD_EVENT_END_REASON_RESERVED);

    g_async_queue_push(queue->queue, g_new0(EventdQueueEvent, 1));

    g_thread_join(queue->thread);

    g_async_queue_unref(queue->queue);

    g_free(queue);
}

void
eventd_queue_push(EventdQueue *queue, EventdEvent *event)
{
    EventdQueueEvent *queue_event;

    queue_event = g_new0(EventdQueueEvent, 1);
    queue_event->event = g_object_ref(event);

    g_async_queue_push_unlocked(queue->queue, queue_event);
}
