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
    GQueue *queue;
    GQueue *current;
};

typedef struct {
    EventdQueue *queue;
    GList *link;
    const gchar *config_id;
    EventdEvent *event;
    gulong answered_handler;
    gulong ended_handler;
    guint timeout_id;
} EventdQueueEvent;

static void _eventd_queue_source_try_dispatch(EventdQueue *queue);

static gboolean
_eventd_queue_event_timeout(gpointer user_data)
{
    EventdQueueEvent *event = user_data;

    event->timeout_id = 0;

    eventd_event_end(event->event, EVENTD_EVENT_END_REASON_TIMEOUT);

    return FALSE;
}

static gboolean
_eventd_queue_event_set_timeout(EventdQueue *queue, const gchar *config_id, EventdQueueEvent *event)
{
    gint64 timeout;

    timeout = eventd_event_get_timeout(event->event);
    if ( timeout < 0 )
        timeout = eventd_config_event_get_timeout(queue->config, config_id);
    eventd_event_set_timeout(event->event, timeout);

    if ( timeout > 0 )
    {
        event->timeout_id = g_timeout_add(timeout, _eventd_queue_event_timeout, event);
        return TRUE;
    }
    return FALSE;
}

static void
_eventd_queue_event_updated(GObject *object, gpointer user_data)
{
    EventdQueueEvent *event = user_data;

    if ( event->timeout_id > 0 )
        g_source_remove(event->timeout_id);
    event->timeout_id = 0;

    _eventd_queue_event_set_timeout(event->queue, event->config_id, event);
}

static void
_eventd_queue_event_ended(GObject *object, gint reason, gpointer user_data)
{
    EventdQueueEvent *event = user_data;
    EventdQueue *queue = event->queue;

    if ( event->timeout_id > 0 )
        g_source_remove(event->timeout_id);

    g_queue_delete_link(queue->current, event->link);
    _eventd_queue_source_try_dispatch(queue);

    g_object_unref(event->event);
    g_free(event);
}

static void
_eventd_queue_source_dispatch(EventdQueue *queue, EventdQueueEvent *event)
{
    g_queue_push_head(queue->current, event);
    event->link = g_queue_peek_head_link(queue->current);

    eventd_plugins_event_action_all(event->config_id, event->event);

    g_signal_connect(event->event, "updated", G_CALLBACK(_eventd_queue_event_updated), event);
    g_signal_connect(event->event, "ended", G_CALLBACK(_eventd_queue_event_ended), event);
    _eventd_queue_event_set_timeout(queue, event->config_id, event);
}

static void
_eventd_queue_source_try_dispatch(EventdQueue *queue)
{
    guint64 stack;

    stack = eventd_config_get_stack(queue->config);
    if ( ( stack > 0 ) && ( g_queue_get_length(queue->current) == stack ) )
        return;

    EventdQueueEvent *event;

    event = g_queue_pop_head(queue->queue);
    if ( event != NULL )
        _eventd_queue_source_dispatch(queue, event);
}

EventdQueue *
eventd_queue_new(EventdConfig *config)
{
    EventdQueue *queue;

    queue = g_new0(EventdQueue, 1);

    queue->config = config;

    queue->queue = g_queue_new();
    queue->current = g_queue_new();

    return queue;
}

static void
#if GLIB_CHECK_VERSION(2,32,0)
_eventd_queue_event_free(gpointer data)
#else /* ! GLIB_CHECK_VERSION(2,32,0) */
_eventd_queue_event_free(gpointer data, gpointer user_data)
#endif /* ! GLIB_CHECK_VERSION(2,32,0) */
{
    EventdQueueEvent *event = data;

    if ( event->answered_handler > 0 )
        g_signal_handler_disconnect(event->event, event->answered_handler);
    if ( event->ended_handler > 0 )
        g_signal_handler_disconnect(event->event, event->ended_handler);
    if ( event->timeout_id > 0 )
        g_source_remove(event->timeout_id);

    g_object_unref(event->event);

    g_free(event);
}

void
eventd_queue_free(EventdQueue *queue)
{
#if GLIB_CHECK_VERSION(2,32,0)
    g_queue_free_full(queue->current, _eventd_queue_event_free);
    g_queue_free_full(queue->queue, _eventd_queue_event_free);
#else /* ! GLIB_CHECK_VERSION(2,32,0) */
    g_queue_foreach(queue->current, _eventd_queue_event_free, NULL);
    g_queue_foreach(queue->queue, _eventd_queue_event_free, NULL);
    g_queue_free(queue->current);
    g_queue_free(queue->queue);
#endif /* ! GLIB_CHECK_VERSION(2,32,0) */

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

    g_queue_push_tail(queue->queue, queue_event);
    _eventd_queue_source_try_dispatch(queue);
}
