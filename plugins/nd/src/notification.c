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
_eventd_nd_notification_set(EventdNdNotification *self, EventdPluginContext *context, EventdEvent *event, cairo_surface_t **bubble)
{
    eventd_event_unref(self->event);
    self->event = eventd_event_ref(event);

    *bubble = eventd_nd_cairo_get_surface(event, self->style, context->max_width, context->max_height);

    self->width = cairo_image_surface_get_width(*bubble);
    self->height = cairo_image_surface_get_height(*bubble);

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
    self->event = eventd_event_ref(event);

    cairo_surface_t *bubble;

    _eventd_nd_notification_set(self, self->context, event, &bubble);

    self->surface = context->backend->surface_new(context->backend->context, self, bubble);

    cairo_surface_destroy(bubble);

    return self;
}

void
eventd_nd_notification_free(gpointer data)
{
    EventdNdNotification *self = data;

    if ( self->timeout > 0 )
        g_source_remove(self->timeout);

    self->context->backend->surface_free(self->surface);

    g_free(self);
}

void
eventd_nd_notification_update(EventdNdNotification *self, EventdEvent *event)
{
    cairo_surface_t *bubble;

    _eventd_nd_notification_set(self, self->context, event, &bubble);

    self->context->backend->surface_update(self->surface, bubble);

    cairo_surface_destroy(bubble);
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
