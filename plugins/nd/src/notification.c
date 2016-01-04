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
    EventdEvent *event;
    struct {
        PangoLayout *title;
        PangoLayout *message;
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
    if ( self->text.title != NULL )
        g_object_unref(self->text.title);
    self->text.title = NULL;
    if ( self->text.message != NULL )
        g_object_unref(self->text.message);
    self->text.message = NULL;
}

static void
_eventd_nd_notification_update(EventdNdNotification *self, EventdEvent *event)
{
    _eventd_nd_notification_clean(self);
    self->event = eventd_event_ref(event);

    gint padding;
    gint min_width, max_width;

    gint text_width = 0, text_height = 0;
    gint image_width = self->context->max_width, image_height = self->context->max_height;


    padding = eventd_nd_style_get_bubble_padding(self->style);
    min_width = eventd_nd_style_get_bubble_min_width(self->style);
    max_width = eventd_nd_style_get_bubble_max_width(self->style);

    /* proccess data and compute the bubble size */
    eventd_nd_cairo_text_process(self->style, self->event, ( max_width > -1 ) ? ( max_width - 2 * padding ) : -1, &self->text.title, &self->text.message, &text_height, &text_width);

    self->width = 2 * padding + text_width;

    if ( ( max_width < 0 ) || ( self->width < max_width ) )
    {
        eventd_nd_cairo_image_and_icon_process(self->style, self->event, ( max_width > -1 ) ? ( max_width - self->width ) : -1, &self->image, &self->icon, &self->text.x, &image_width, &image_height);
        self->width += image_width;
    }

    /* We are sure that min_width < max_width */
    if ( min_width > self->width )
    {
        self->width = min_width;
        /* Let the text take the remaining space if needed (e.g. Right-to-Left) */
        text_width = ( self->width - ( 2 * padding + image_width ) );
    }
    pango_layout_set_width(self->text.title, text_width * PANGO_SCALE);
    if ( self->text.message != NULL )
        pango_layout_set_width(self->text.message, text_width * PANGO_SCALE);

    self->height = 2 * padding + MAX(image_height, text_height);

    if ( self->timeout > 0 )
        g_source_remove(self->timeout);
    self->timeout = g_timeout_add_full(G_PRIORITY_DEFAULT, eventd_nd_style_get_bubble_timeout(self->style), _eventd_nd_event_timedout, self, NULL);
}

EventdNdNotification *
eventd_nd_notification_new(EventdPluginContext *context, EventdEvent *event, EventdNdStyle *style)
{
    EventdNdNotification *self;

    self = g_new0(EventdNdNotification, 1);
    self->context = context;
    self->style = style;

    _eventd_nd_notification_update(self, event);
    self->surface = self->context->backend->surface_new(self->context->backend->context, self, self->width, self->height);

    return self;
}

void
eventd_nd_notification_free(gpointer data)
{
    EventdNdNotification *self = data;

    if ( self->timeout > 0 )
        g_source_remove(self->timeout);

    self->context->backend->surface_free(self->surface);
    _eventd_nd_notification_clean(self);

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
    eventd_nd_cairo_text_draw(cr, self->style, self->text.title, self->text.message, padding + self->text.x, padding, self->height - ( 2 * padding ));
    cairo_destroy(cr);
}

void
eventd_nd_notification_update(EventdNdNotification *self, EventdEvent *event)
{
    _eventd_nd_notification_update(self, event);
    self->context->backend->surface_update(self->surface, self->width, self->height);
}

void
eventd_nd_notification_dismiss(EventdNdNotification *self)
{
    EventdEvent *event;
    event = eventd_event_new(".notification", "dismiss");
    eventd_event_add_data(event, g_strdup("source-event"), g_strdup(eventd_event_get_uuid(self->event)));
    eventd_plugin_core_push_event(self->context->core, event);
    eventd_event_unref(event);
}
