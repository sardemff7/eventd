/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2024 Morgane "Sardem FF7" Glidic
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

#include "config.h"

#include <glib.h>
#include <glib-object.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <cairo.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>

#include "eventd-plugin.h"
#include "libeventd-event.h"
#include "libeventd-helpers-config.h"

#include "backend.h"
#include "style.h"
#include "draw.h"
#include "nd.h"

#include "notification.h"

typedef struct {
    gint x;
    gint y;
} Point;

typedef struct {
    gint width;
    gint height;
} Size;

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
        gint height;
    } text;
    cairo_surface_t *image;
    cairo_surface_t *icon;
    Point offset;
    Size surface_size;
    Size border_size;
    Size bubble_size;
    Size content_size;
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
    eventd_event_add_data_string(event, g_strdup("source-event"), g_strdup(eventd_event_get_uuid(self->event)));
    eventd_plugin_core_push_event(self->context->core, event);
    eventd_event_unref(event);

    return G_SOURCE_REMOVE;
}

static void
_eventd_nd_notification_clean(EventdNdNotification *self)
{
    eventd_event_unref(self->event);

    if ( self->icon != NULL )
        cairo_surface_destroy(self->icon);
    self->icon = NULL;
    if ( self->image != NULL )
        cairo_surface_destroy(self->image);
    self->image = NULL;
    if ( self->text.text != NULL )
        g_object_unref(self->text.text);
    self->text.text = NULL;
}

static void
_eventd_nd_notification_process(EventdNdNotification *self, EventdEvent *event)
{
    _eventd_nd_notification_clean(self);
    if ( event != NULL )
        self->event = eventd_event_ref(event);

    gint border, padding;
    gint progress_bar_width = 0;
    gint min_width, max_width;

    gint text_width = 0, text_max_width;
    gint image_width = 0, image_height = 0;


    switch ( self->context->shaping )
    {
    case EVENTD_ND_SHAPING_NONE:
    case EVENTD_ND_SHAPING_SHAPE:
        self->offset.x = 0;
        self->offset.y = 0;
        self->surface_size.width = 0;
        self->surface_size.height = 0;
    break;
    case EVENTD_ND_SHAPING_COMPOSITING:
    {
        gint blur, offset_x, offset_y;
        blur = eventd_nd_style_get_bubble_border_blur(self->style) * 2; /* We must reserve enough space to avoid clipping */
        offset_x = eventd_nd_style_get_bubble_border_blur_offset_x(self->style);
        offset_y = eventd_nd_style_get_bubble_border_blur_offset_y(self->style);

        self->offset.x = MAX(0, blur - offset_x);
        self->offset.y = MAX(0, blur - offset_y);
        self->surface_size.width = 2 * blur + MAX(0, ABS(offset_x) - blur);
        self->surface_size.height = 2 * blur + MAX(0, ABS(offset_y) - blur);
    }
    break;
    }
    border = eventd_nd_style_get_bubble_border(self->style);
    padding = eventd_nd_style_get_bubble_padding(self->style);
    min_width = eventd_nd_style_get_bubble_min_width(self->style);
    max_width = eventd_nd_style_get_bubble_max_width(self->style);

    switch ( eventd_nd_style_get_progress_placement(self->style) )
    {
    case EVENTD_ND_STYLE_PROGRESS_PLACEMENT_BAR_BOTTOM:
        if ( self->event != NULL )
        {
            GVariant *val;

            val = eventd_event_get_data(self->event, eventd_nd_style_get_template_progress(self->style));
            if ( val != NULL )
                progress_bar_width = eventd_nd_style_get_progress_bar_width(self->style);
        }
    break;
    case EVENTD_ND_STYLE_PROGRESS_PLACEMENT_IMAGE_BOTTOM_TOP:
    case EVENTD_ND_STYLE_PROGRESS_PLACEMENT_IMAGE_TOP_BOTTOM:
    case EVENTD_ND_STYLE_PROGRESS_PLACEMENT_IMAGE_LEFT_RIGHT:
    case EVENTD_ND_STYLE_PROGRESS_PLACEMENT_IMAGE_RIGHT_LEFT:
    case EVENTD_ND_STYLE_PROGRESS_PLACEMENT_IMAGE_CIRCULAR:
    break;
    }

    if ( max_width < 0 )
        max_width = self->context->geometry.w - 2 * ( self->queue->margin_x + border );
    max_width -= 2 * padding;
    min_width += 2 * padding;
    if ( min_width > max_width )
        min_width = max_width;

    /* proccess data and compute the bubble size */
    text_max_width = eventd_nd_style_get_text_max_width(self->style);
    if ( text_max_width < 0 )
        text_max_width = max_width;
    else
        text_max_width = MIN(text_max_width, max_width);
    self->text.text = eventd_nd_draw_text_process(self->style, self->event, text_max_width, g_queue_get_length(self->queue->wait_queue), &text_width);

    self->content_size.width = text_width;

    if ( self->content_size.width < max_width )
    {
        if ( self->event != NULL )
            eventd_nd_draw_image_and_icon_process(self->context->theme_context, self->style, self->event, max_width - self->content_size.width, self->context->geometry.s, &self->image, &self->icon, &self->text.x, &image_width, &image_height);
        self->content_size.width += image_width;
    }

    /* We are sure that min_width <= max_width */
    if ( min_width > self->content_size.width )
    {
        self->content_size.width = min_width;
        /* Let the text take the remaining space if needed (e.g. Right-to-Left) */
        text_width = self->content_size.width - image_width;
    }
    pango_layout_set_width(self->text.text, text_width * PANGO_SCALE);
    pango_layout_get_pixel_size(self->text.text, NULL, &self->text.height);

    self->content_size.height = MAX(image_height, self->text.height);

    self->bubble_size.width = self->content_size.width + 2 * padding;
    self->bubble_size.height = self->content_size.height + 2 * padding + progress_bar_width;
    self->border_size.width = self->bubble_size.width + 2 * border;
    self->border_size.height = self->bubble_size.height + 2 * border;
    self->surface_size.width += self->border_size.width;
    self->surface_size.height += self->border_size.height;

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
    self->context->backend->surface_update(self->surface, self->surface_size.width, self->surface_size.height);
}

static void
_eventd_nd_notification_refresh_list(EventdPluginContext *context, EventdNdQueue *queue)
{
    if ( queue->more_notification != NULL )
    {
        g_queue_pop_tail_link(queue->queue);
        queue->more_notification->visible = FALSE;
    }

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
                queue->more_notification = eventd_nd_notification_new(context, NULL, context->style);
            else
            {
                _eventd_nd_notification_update(queue->more_notification, NULL);
                g_queue_push_tail_link(queue->queue, queue->more_notification->link);
            }
            queue->more_notification->visible = TRUE;
        }
        else if ( queue->more_notification != NULL )
            eventd_nd_notification_free(queue->more_notification);
    }

    gpointer data = NULL;
    if ( context->backend->move_begin != NULL )
        data = context->backend->move_begin(context->backend->context, g_queue_get_length(queue->queue));

    gboolean right, center, bottom;
    right = ( queue->anchor == EVENTD_ND_ANCHOR_TOP_RIGHT ) || ( queue->anchor == EVENTD_ND_ANCHOR_BOTTOM_RIGHT );
    center = ( queue->anchor == EVENTD_ND_ANCHOR_TOP ) || ( queue->anchor == EVENTD_ND_ANCHOR_BOTTOM );
    bottom = ( queue->anchor == EVENTD_ND_ANCHOR_BOTTOM_LEFT ) || ( queue->anchor == EVENTD_ND_ANCHOR_BOTTOM ) || ( queue->anchor == EVENTD_ND_ANCHOR_BOTTOM_RIGHT );

    gint bx, by;
    bx = queue->margin_x;
    by = queue->margin_y;
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
            by -= self->border_size.height;

        gint x, y;
        x = center ? ( ( bx / 2 ) - ( self->border_size.width / 2 ) ) : right ? ( bx - self->border_size.width ) : bx;
        y = by;
        x -= self->offset.x;
        y -= self->offset.y;
        context->backend->move_surface(self->surface, x, y, data);

        if ( bottom )
            by -= queue->spacing;
        else
            by += self->border_size.height + queue->spacing;
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
    self->queue = g_hash_table_lookup(context->queues, eventd_nd_style_get_bubble_queue(style));
    if ( self->queue == NULL )
        self->queue = g_hash_table_lookup(context->queues, "default");
    self->style = style;

    if ( event != NULL )
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
    self->surface = self->context->backend->surface_new(self->context->backend->context, self, self->surface_size.width, self->surface_size.height);
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
    else if ( self->event == NULL )
        g_list_free_1(self->link);
    else
        g_queue_delete_link(self->queue->wait_queue, self->link);

    if ( self->event == NULL )
        self->queue->more_notification = NULL;

    self->context->backend->surface_free(self->surface);
    _eventd_nd_notification_clean(self);

    if ( ( ! self->context->no_refresh ) && ( self->visible ) )
        _eventd_nd_notification_refresh_list(self->context, self->queue);

    g_free(self);
}

void
eventd_nd_notification_shape(EventdNdNotification *self, cairo_t *cr)
{
    gint border;

    border = eventd_nd_style_get_bubble_border(self->style);
    cairo_translate(cr, border, border);

    eventd_nd_draw_bubble_shape(cr, self->style, self->bubble_size.width, self->bubble_size.height);
}

void
eventd_nd_notification_draw(EventdNdNotification *self, cairo_surface_t *surface)
{
    gint padding;
    gint offset_y = 0;
    gdouble value = -1;

    padding = eventd_nd_style_get_bubble_padding(self->style);

    switch ( eventd_nd_style_get_text_valign(self->style) )
    {
    case EVENTD_ND_VANCHOR_BOTTOM:
        offset_y = self->content_size.height - self->text.height;
    break;
    case EVENTD_ND_VANCHOR_CENTER:
        offset_y = self->content_size.height / 2 - self->text.height / 2;
    break;
    case EVENTD_ND_VANCHOR_TOP:
    break;
    }

    if ( self->event != NULL )
    {
        GVariant *val;

        val = eventd_event_get_data(self->event, eventd_nd_style_get_template_progress(self->style));
        if ( val != NULL )
            value = g_variant_get_double(val);
        if ( eventd_nd_style_get_progress_reversed(self->style) )
            value = 1.0 - value;
    }

    cairo_t *cr;
    cr = cairo_create(surface);

    eventd_nd_draw_bubble_draw(cr, self->style, self->bubble_size.width, self->bubble_size.height, self->context->shaping, value);
    cairo_translate(cr, padding, padding);
    eventd_nd_draw_image_and_icon_draw(cr, self->image, self->icon, self->style, self->content_size.width, self->content_size.height, value);
    eventd_nd_draw_text_draw(cr, self->style, self->text.text, self->text.x, offset_y);

    cairo_destroy(cr);
    cairo_surface_flush(surface);
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
    if ( self->event == NULL )
    {
        eventd_nd_notification_dismiss_target(self->context, EVENTD_ND_DISMISS_ALL, self->queue);
        return;
    }

    EventdEvent *event;
    event = eventd_event_new(".notification", "dismiss");
    eventd_event_add_data_string(event, g_strdup("source-event"), g_strdup(eventd_event_get_uuid(self->event)));
    eventd_plugin_core_push_event(self->context->core, event);
    eventd_event_unref(event);
}

void
eventd_nd_notification_dismiss_queue(EventdNdNotification *self)
{
    eventd_nd_notification_dismiss_target(self->context, EVENTD_ND_DISMISS_ALL, self->queue);
}


void
eventd_nd_notification_refresh_list(EventdPluginContext *context, gboolean update)
{
    GHashTableIter iter;
    if ( update )
    {
        EventdNdNotification *notification;
        g_hash_table_iter_init(&iter, context->notifications);
        while ( g_hash_table_iter_next(&iter, NULL, (gpointer *) &notification) )
        {
            /* We may be the last user of event, so make sure we keep it alive */
            _eventd_nd_notification_update(notification, eventd_event_ref(notification->event));
            eventd_event_unref(notification->event);
        }
    }

    EventdNdQueue *queue;
    g_hash_table_iter_init(&iter, context->queues);
    while ( g_hash_table_iter_next(&iter, NULL, (gpointer *) &queue) )
        _eventd_nd_notification_refresh_list(context, queue);
}

EventdPluginCommandStatus
eventd_nd_notification_dismiss_target(EventdPluginContext *context, EventdNdDismissTarget target, EventdNdQueue *queue)
{
    if ( queue == NULL )
    {
        gboolean r = FALSE;
        GHashTableIter iter;
        g_hash_table_iter_init(&iter, context->queues);
        while ( g_hash_table_iter_next(&iter, NULL, (gpointer *) &queue) )
            r = ( eventd_nd_notification_dismiss_target(context, target, queue) == EVENTD_PLUGIN_COMMAND_STATUS_OK ) || r;
        return r ? EVENTD_PLUGIN_COMMAND_STATUS_OK : EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_1;
    }

    GList *notification = NULL;

    switch ( target )
    {
    case EVENTD_ND_DISMISS_NONE:
    break;
    case EVENTD_ND_DISMISS_ALL:
    {
        gboolean r = FALSE;
        notification = g_queue_peek_head_link(queue->queue);
        while ( notification != NULL )
        {
            GList *next = g_list_next(notification);

            eventd_nd_notification_dismiss(notification->data);

            notification = next;
            r = TRUE;
        }
        return r ? EVENTD_PLUGIN_COMMAND_STATUS_OK : EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_1;
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

    if ( notification == NULL )
        return EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_1;

    eventd_nd_notification_dismiss(notification->data);
    return EVENTD_PLUGIN_COMMAND_STATUS_OK;
}
