/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2015 Quentin "Sardem FF7" Glidic
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

#include <config.h>

#include <glib.h>
#include <glib-object.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <cairo.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>

#include <eventd-plugin.h>
#include <libeventd-event.h>
#include <libeventd-helpers-config.h>

#include "backend.h"
#include "style.h"
#include "cairo.h"
#include "icon.h"
#include "nd.h"

#include "notification.h"


struct _EventdNdNotification {
    EventdNdContext *context;
    EventdNdStyle *style;
    EventdNdQueue *queue;
    GList *link;
    gboolean visible;
    EventdEvent *event;
    struct {
        PangoLayout *text;
        gint x;
    } text;
    cairo_surface_t *image;
    cairo_surface_t *icon;
    gint width;
    gint height;
    guint timeout;
    EventdNdSurface *surface;
};


static gboolean
_eventd_nd_event_timedout(gpointer user_data)
{
    EventdNdNotification *self = user_data;
    EventdEvent *event;

    self->timeout = 0;
    event = eventd_event_new(".notification", "timeout");
    eventd_event_add_data(event, g_strdup("source-event"), g_strdup(eventd_event_get_uuid(self->event)));
    eventd_plugin_core_push_event(self->context->core, event);
    eventd_event_unref(event);

    return FALSE;
}

static void
_eventd_nd_notification_clean(EventdNdNotification *self)
{
    eventd_event_unref(self->event);
    if ( self->text.text != NULL )
        g_object_unref(self->text.text);
    self->text.text = NULL;
}

static void
_eventd_nd_notification_process(EventdNdNotification *self, EventdEvent *event)
{
    _eventd_nd_notification_clean(self);
    self->event = eventd_event_ref(event);

    gint margin, padding;
    gint min_width, max_width;

    gint text_width = 0, text_height = 0;
    gint image_width = self->context->max_width, image_height = self->context->max_height;


    margin = self->queue->margin;
    padding = eventd_nd_style_get_bubble_padding(self->style);
    min_width = eventd_nd_style_get_bubble_min_width(self->style);
    max_width = eventd_nd_style_get_bubble_max_width(self->style);
    if ( max_width < 0 )
        max_width = self->context->geometry.w - 2 * margin;
    if ( min_width > max_width )
        min_width = max_width;

    /* proccess data and compute the bubble size */
    self->text.text = eventd_nd_cairo_text_process(self->style, self->event, max_width - 2 * padding, &text_height, &text_width);

    self->width = 2 * padding + text_width;

    if ( self->width < max_width )
    {
        eventd_nd_cairo_image_and_icon_process(self->style, self->event, max_width - self->width, &self->image, &self->icon, &self->text.x, &image_width, &image_height);
        self->width += image_width;
    }

    /* We are sure that min_width <= max_width */
    if ( min_width > self->width )
    {
        self->width = min_width;
        /* Let the text take the remaining space if needed (e.g. Right-to-Left) */
        text_width = ( self->width - ( 2 * padding + image_width ) );
    }
    pango_layout_set_width(self->text.text, text_width * PANGO_SCALE);

    self->height = 2 * padding + MAX(image_height, text_height);

    if ( self->timeout > 0 )
    {
        g_source_remove(self->timeout);
        self->timeout = g_timeout_add_full(G_PRIORITY_DEFAULT, eventd_nd_style_get_bubble_timeout(self->style), _eventd_nd_event_timedout, self, NULL);
    }
}

static void
_eventd_nd_notification_update(EventdNdNotification *self, EventdEvent *event)
{
    _eventd_nd_notification_process(self, event);
    self->context->backend->surface_update(self->surface, self->width, self->height);
}

static void
_eventd_nd_notification_refresh_list(EventdPluginContext *context, EventdNdQueue *queue)
{
    if ( queue->more_notification != NULL )
        g_queue_pop_tail_link(queue->queue);

    while ( ( g_queue_get_length(queue->queue) < queue->limit ) && ( ! g_queue_is_empty(queue->wait_queue) ) )
    {
        GList *link;
        link = g_queue_pop_head_link(queue->wait_queue);
        if ( queue->reverse )
            g_queue_push_tail_link(queue->queue, link);
        else
            g_queue_push_head_link(queue->queue, link);

        EventdNdNotification *self = link->data;
        gint timeout;
        timeout = eventd_nd_style_get_bubble_timeout(self->style);
        if ( timeout > 0 )
            self->timeout = g_timeout_add_full(G_PRIORITY_DEFAULT, timeout, _eventd_nd_event_timedout, self, NULL);
        self->visible = TRUE;
    }

    if ( queue->more_indicator )
    {
        if ( ! g_queue_is_empty(queue->wait_queue) )
        {
            if ( queue->more_notification == NULL )
            {
                queue->more_event = eventd_event_new("eventd-nd-more", eventd_nd_anchors[queue->anchor]);
                if ( eventd_plugin_core_push_event(context->core, queue->more_event) )
                    return;
                g_warning("“More” indicator configured, but missing event configuration: disabling indicator");
                queue->more_indicator = FALSE;
                eventd_event_unref(queue->more_event);
                queue->more_event = NULL;
            }
            else
            {
                eventd_event_add_data(queue->more_event, g_strdup("size"), g_strdup_printf("%u", g_queue_get_length(queue->wait_queue)));
                _eventd_nd_notification_update(queue->more_notification, queue->more_event);
                g_queue_push_tail_link(queue->queue, queue->more_notification->link);
            }
        }
        else if ( queue->more_notification != NULL )
            g_hash_table_remove(context->notifications, eventd_event_get_uuid(queue->more_event));
    }

    gpointer data = NULL;
    if ( context->backend->move_begin != NULL )
        data = context->backend->move_begin(context->backend->context, g_queue_get_length(queue->queue));

    gboolean right, center, bottom;
    right = ( queue->anchor == EVENTD_ND_ANCHOR_TOP_RIGHT ) || ( queue->anchor == EVENTD_ND_ANCHOR_BOTTOM_RIGHT );
    center = ( queue->anchor == EVENTD_ND_ANCHOR_TOP ) || ( queue->anchor == EVENTD_ND_ANCHOR_BOTTOM );
    bottom = ( queue->anchor == EVENTD_ND_ANCHOR_BOTTOM_LEFT ) || ( queue->anchor == EVENTD_ND_ANCHOR_BOTTOM ) || ( queue->anchor == EVENTD_ND_ANCHOR_BOTTOM_RIGHT );

    gint bx, by;
    bx = queue->margin;
    by = queue->margin;
    if ( center )
        bx = context->geometry.w;
    else if ( right )
        bx = context->geometry.w - bx;
    if ( bottom )
        by = context->geometry.h - by;
    GList *self_;
    for ( self_ = g_queue_peek_head_link(queue->queue) ; self_ != NULL ; self_ = g_list_next(self_) )
    {
        EventdNdNotification *self = self_->data;

        if ( bottom )
            by -= self->height;

        gint x, y;
        x = center ? ( ( bx / 2 ) - ( self->width / 2 ) ) : right ? ( bx - self->width ) : bx;
        y = by;
        context->backend->move_surface(self->surface, x, y, data);

        if ( bottom )
            by -= queue->spacing;
        else
            by += self->height + queue->spacing;
    }

    if ( context->backend->move_end != NULL )
        context->backend->move_end(context->backend->context, data);
}

EventdNdNotification *
eventd_nd_notification_new(EventdPluginContext *context, EventdEvent *event, EventdNdStyle *style)
{
    EventdNdNotification *self;

    self = g_new0(EventdNdNotification, 1);
    self->context = context;
    self->queue = &context->queues[eventd_nd_style_get_bubble_anchor(style)];
    self->style = style;

    if ( self->queue->more_event != event )
    {
        g_queue_push_tail(self->queue->wait_queue, self);
        self->link = g_queue_peek_tail_link(self->queue->wait_queue);
    }
    else
    {
        self->queue->more_notification = self;
        g_queue_push_tail(self->queue->queue, self);
        self->link = g_queue_peek_tail_link(self->queue->queue);
    }

    _eventd_nd_notification_process(self, event);
    self->surface = self->context->backend->surface_new(self->context->backend->context, self, self->width, self->height);
    _eventd_nd_notification_refresh_list(self->context, self->queue);

    return self;
}

void
eventd_nd_notification_free(gpointer data)
{
    EventdNdNotification *self = data;

    if ( self->timeout > 0 )
        g_source_remove(self->timeout);

    if ( self->visible )
        g_queue_delete_link(self->queue->queue, self->link);
    else
        g_queue_delete_link(self->queue->wait_queue, self->link);

    if ( self->event == self->queue->more_event )
    {
        eventd_event_unref(self->queue->more_event);
        self->queue->more_event = NULL;
        self->queue->more_notification = NULL;
    }

    self->context->backend->surface_free(self->surface);
    _eventd_nd_notification_clean(self);

    _eventd_nd_notification_refresh_list(self->context, self->queue);

    g_free(self);
}

void
eventd_nd_notification_draw(EventdNdNotification *self, cairo_surface_t *bubble)
{
    gint padding;
    padding = eventd_nd_style_get_bubble_padding(self->style);

    cairo_t *cr;
    cr = cairo_create(bubble);
    eventd_nd_cairo_bubble_draw(cr, eventd_nd_style_get_bubble_colour(self->style), eventd_nd_style_get_bubble_radius(self->style), self->width, self->height);
    eventd_nd_cairo_image_and_icon_draw(cr, self->image, self->icon, self->style, self->width, self->height);
    eventd_nd_cairo_text_draw(cr, self->style, self->text.text, padding + self->text.x, padding, self->height - ( 2 * padding ));
    cairo_destroy(cr);
}

void
eventd_nd_notification_update(EventdNdNotification *self, EventdEvent *event)
{
    _eventd_nd_notification_update(self, event);
    _eventd_nd_notification_refresh_list(self->context, self->queue);
}

void
eventd_nd_notification_dismiss(EventdNdNotification *self)
{
    if ( self->queue->more_event == self->event )
        return;

    EventdEvent *event;
    event = eventd_event_new(".notification", "dismiss");
    eventd_event_add_data(event, g_strdup("source-event"), g_strdup(eventd_event_get_uuid(self->event)));
    eventd_plugin_core_push_event(self->context->core, event);
    eventd_event_unref(event);
}


void
eventd_nd_notification_geometry_changed(EventdPluginContext *context, gboolean resize)
{
    if ( resize )
    {
        GHashTableIter iter;
        EventdNdNotification *notification;
        g_hash_table_iter_init(&iter, context->notifications);
        while ( g_hash_table_iter_next(&iter, NULL, (gpointer *) &notification) )
        {
            /* We may be the last user of event, so make sure we keep it alive */
            _eventd_nd_notification_update(notification, eventd_event_ref(notification->event));
            eventd_event_unref(notification->event);
        }
    }

    EventdNdAnchor i;
    for ( i = EVENTD_ND_ANCHOR_TOP_LEFT ; i < _EVENTD_ND_ANCHOR_SIZE ; ++i )
        _eventd_nd_notification_refresh_list(context, &context->queues[i]);
}

void
eventd_nd_notification_dismiss_target(EventdPluginContext *context, EventdNdDismissTarget target, EventdNdAnchor anchor)
{
    if ( anchor == _EVENTD_ND_ANCHOR_SIZE )
    {
        for ( anchor = EVENTD_ND_ANCHOR_TOP_LEFT ; anchor < _EVENTD_ND_ANCHOR_SIZE ; ++anchor )
            eventd_nd_notification_dismiss_target(context, target, anchor);
        return;
    }

    EventdNdQueue *queue = &context->queues[anchor];
    GList *notification = NULL;

    switch ( target )
    {
    case EVENTD_ND_DISMISS_NONE:
    break;
    case EVENTD_ND_DISMISS_ALL:
    {
        notification = g_queue_peek_head_link(queue->queue);
        while ( notification != NULL )
        {
            GList *next = g_list_next(notification);

            eventd_nd_notification_dismiss(notification->data);

            notification = next;
        }
        return;
    }
    case EVENTD_ND_DISMISS_OLDEST:
    {
        if ( queue->reverse )
            notification = g_queue_peek_head_link(queue->queue);
        else
            notification = g_queue_peek_tail_link(queue->queue);
    }
    break;
    case EVENTD_ND_DISMISS_NEWEST:
    {
        if ( queue->reverse )
            notification = g_queue_peek_tail_link(queue->queue);
        else
            notification = g_queue_peek_head_link(queue->queue);
    }
    break;
    }
    if ( notification != NULL )
        eventd_nd_notification_dismiss(notification->data);
}
