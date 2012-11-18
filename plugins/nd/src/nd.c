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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <glib.h>
#include <glib-object.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <pango/pango.h>
#include <cairo.h>

#include <eventd-plugin.h>
#include <libeventd-event.h>
#include <libeventd-config.h>
#include <libeventd-nd-notification.h>
#include <libeventd-nd-notification-template.h>

#include <eventd-nd-backend.h>

#include "backends.h"
#include "style.h"
#include "cairo.h"

typedef enum {
    EVENTD_ND_ANCHOR_TOP_LEFT     = EVENTD_ND_ANCHOR_TOP    | EVENTD_ND_ANCHOR_LEFT,
    EVENTD_ND_ANCHOR_TOP_RIGHT    = EVENTD_ND_ANCHOR_TOP    | EVENTD_ND_ANCHOR_RIGHT,
    EVENTD_ND_ANCHOR_BOTTOM_LEFT  = EVENTD_ND_ANCHOR_BOTTOM | EVENTD_ND_ANCHOR_LEFT,
    EVENTD_ND_ANCHOR_BOTTOM_RIGHT = EVENTD_ND_ANCHOR_BOTTOM | EVENTD_ND_ANCHOR_RIGHT,
    EVENTD_ND_ANCHOR_REVERSE      = 1<<4
} EventdNdCornerAnchor;

struct _EventdPluginContext {
    EventdNdInterface interface;
    GHashTable *events;
    EventdNdCornerAnchor bubble_anchor;
    gint bubble_margin;
    gint max_width;
    gint max_height;
    EventdNdStyle *style;
    GHashTable *backends;
    GList *displays;
    GQueue *queue;
};

typedef struct {
    EventdNdBackend *backend;
    EventdNdDisplay *display;
} EventdNdDisplayContext;

typedef struct {
    EventdNdBackend *backend;
    EventdNdSurface *surface;
} EventdNdSurfaceContext;

typedef struct {
    EventdNdContext *context;
    EventdNdStyle *style;
    GList *notification;
    gint width;
    gint height;
    GList *surfaces;
} EventdNdNotification;


static void
_eventd_nd_backend_display_free(gpointer data)
{
    EventdNdDisplayContext *display = data;

    display->backend->display_free(display->display);

    g_free(display);
}

static void
_eventd_nd_backend_remove_display(EventdNdContext *context, EventdNdDisplay *display)
{
    EventdNdDisplayContext *display_context;
    GList *display_context_;
    for ( display_context_ = context->displays ; display_context_ != NULL ; display_context_ = g_list_next(display_context_) )
    {
        display_context = display_context_->data;
        if ( display_context->display == display )
        {
            _eventd_nd_backend_display_free(display_context);
            context->displays = g_list_delete_link(context->displays, display_context_);
            return;
        }
    }
}

/*
 * Initialization interface
 */

static EventdPluginContext *
_eventd_nd_init(EventdCoreContext *core, EventdCoreInterface *interface)
{
    EventdPluginContext *context;

    context = g_new0(EventdPluginContext, 1);

    context->interface.remove_display = _eventd_nd_backend_remove_display;

    context->backends = eventd_nd_backends_load(context, &context->interface);

    context->events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, eventd_nd_style_free);

    context->queue = g_queue_new();

    libeventd_nd_notification_init();

    eventd_nd_cairo_init();

    /* default bubble position */
    context->bubble_anchor    = EVENTD_ND_ANCHOR_TOP_RIGHT;
    context->bubble_margin    = 13;

    context->style = eventd_nd_style_new(NULL);

    return context;
}

static void
_eventd_nd_uninit(EventdPluginContext *context)
{
    eventd_nd_style_free(context->style);

    eventd_nd_cairo_uninit();

    libeventd_nd_notification_uninit();

    g_queue_free(context->queue);

    g_hash_table_unref(context->events);

    g_list_free_full(context->displays, _eventd_nd_backend_display_free);

    g_hash_table_unref(context->backends);

    g_free(context);
}


/*
 * Control command interface
 */

static void
_eventd_nd_control_command(EventdPluginContext *context, const gchar *command)
{
    const gchar *target = command + strlen("notification-daemon ");
    EventdNdDisplay *display;
    GHashTableIter iter;
    const gchar *id;
    EventdNdBackend *backend;

    if ( ! g_str_has_prefix(command, "notification-daemon ") )
        return;

    g_hash_table_iter_init(&iter, context->backends);
    while ( g_hash_table_iter_next(&iter, (gpointer *)&id, (gpointer *)&backend) )
    {
        if ( ! backend->display_test(backend->context, target) )
            continue;

        display = backend->display_new(backend->context, target);
        if ( display == NULL )
            g_warning("Couldn't initialize display for '%s'", target);
        else
        {
            EventdNdDisplayContext *display_context;

            display_context = g_new(EventdNdDisplayContext, 1);
            display_context->backend = backend;
            display_context->display = display;

            context->displays = g_list_prepend(context->displays, display_context);
        }
    }
}


/*
 * Configuration interface
 */

static void
_eventd_nd_global_parse(EventdPluginContext *context, GKeyFile *config_file)
{
    if ( g_key_file_has_group(config_file, "Notification") )
    {
        Int integer;
        gchar *string;
        gboolean boolean;

        if ( libeventd_config_key_file_get_string(config_file, "Notification", "Anchor", &string) == 0 )
        {
            if ( g_strcmp0(string, "top left") == 0 )
                context->bubble_anchor = EVENTD_ND_ANCHOR_TOP_LEFT;
            else if ( g_strcmp0(string, "top right") == 0 )
                context->bubble_anchor = EVENTD_ND_ANCHOR_TOP_RIGHT;
            else if ( g_strcmp0(string, "bottom left") == 0 )
                context->bubble_anchor = EVENTD_ND_ANCHOR_BOTTOM_LEFT;
            else if ( g_strcmp0(string, "bottom right") == 0 )
                context->bubble_anchor = EVENTD_ND_ANCHOR_BOTTOM_RIGHT;
            else
                g_warning("Wrong anchor value '%s'", string);
            g_free(string);
        }

        if ( ( libeventd_config_key_file_get_boolean(config_file, "Notification", "OldestFirst", &boolean) == 0 ) && boolean )
            context->bubble_anchor |= EVENTD_ND_ANCHOR_REVERSE;

        if ( libeventd_config_key_file_get_int(config_file, "Notification", "Margin", &integer) == 0 )
            context->bubble_margin = integer.value;
    }

    eventd_nd_style_update(context->style, config_file, &context->max_width, &context->max_height);

    GHashTableIter iter;
    const gchar *id;
    EventdNdBackend *backend;
    g_hash_table_iter_init(&iter, context->backends);
    while ( g_hash_table_iter_next(&iter, (gpointer *)&id, (gpointer *)&backend) )
    {
        if ( backend->global_parse != NULL )
            backend->global_parse(backend->context, config_file);
    }
}

static void
_eventd_nd_event_parse(EventdPluginContext *context, const gchar *id, GKeyFile *config_file)
{
    gboolean disable;
    EventdNdStyle *style = NULL;

    if ( ! g_key_file_has_group(config_file, "Notification") )
        return;

    if ( libeventd_config_key_file_get_boolean(config_file, "Notification", "Disable", &disable) < 0 )
        return;

    if ( ! disable )
    {
        style = eventd_nd_style_new(context->style);
        eventd_nd_style_update(style, config_file, &context->max_width, &context->max_height);
    }

    g_hash_table_insert(context->events, g_strdup(id), style);
}

static void
_eventd_nd_config_reset(EventdPluginContext *context)
{
    g_hash_table_remove_all(context->events);
}


/*
 * Event action interface
 */

static EventdNdNotification *
_eventd_nd_notification_new(EventdPluginContext *context, EventdEvent *event, EventdNdStyle *style)
{
    EventdNdNotification *self;

    self = g_new0(EventdNdNotification, 1);
    self->context = context;
    self->style = style;

    LibeventdNdNotification *notification;
    cairo_surface_t *bubble;
    cairo_surface_t *shape;

    notification = libeventd_nd_notification_new(eventd_nd_style_get_template(style), event, context->max_width, context->max_height);
    eventd_nd_cairo_get_surfaces(event, notification, style, &bubble, &shape);
    libeventd_nd_notification_free(notification);

    self->width = cairo_image_surface_get_width(bubble);
    self->height = cairo_image_surface_get_height(bubble);

    GList *display_;
    for ( display_ = context->displays ; display_ != NULL ; display_ = g_list_next(display_) )
    {
        EventdNdDisplayContext *display = display_->data;
        EventdNdSurfaceContext *surface;

        surface = g_new(EventdNdSurfaceContext, 1);
        surface->backend = display->backend;
        surface->surface = display->backend->surface_new(event, display->display, bubble, shape);

        self->surfaces = g_list_prepend(self->surfaces, surface);
    }

    cairo_surface_destroy(bubble);
    cairo_surface_destroy(shape);

    return self;
}

static void
_eventd_nd_notification_surface_context_free(gpointer data)
{
    EventdNdSurfaceContext *surface = data;

    surface->backend->surface_free(surface->surface);

    g_free(surface);
}

static void
_eventd_nd_notification_free(EventdNdNotification *self)
{
    g_list_free_full(self->surfaces, _eventd_nd_notification_surface_context_free);

    g_free(self);
}

static void
_eventd_nd_update_notifications(EventdPluginContext *context)
{
    GList *notification_;
    GList *surface_;
    EventdNdNotification *notification;
    EventdNdSurfaceContext *surface;

    gboolean right;
    gboolean bottom;
    right = context->bubble_anchor & EVENTD_ND_ANCHOR_RIGHT;
    bottom = context->bubble_anchor & EVENTD_ND_ANCHOR_BOTTOM;

    gint x, y;
    x = y = context->bubble_margin;
    if ( right )
        x *= -1;
    if ( bottom )
        y *= -1;
    for ( notification_ = g_queue_peek_head_link(context->queue) ; notification_ != NULL ; notification_ = g_list_next(notification_) )
    {
        notification = notification_->data;
        if ( bottom )
            y -= notification->height;
        if ( right )
            x -= notification->width;
        for ( surface_ = notification->surfaces ; surface_ != NULL ; surface_ = g_list_next(surface_) )
        {
            surface = surface_->data;
            surface->backend->surface_display(surface->surface, x, y);
        }
        if ( right )
            x += notification->width;
        if ( bottom )
            y -= context->bubble_margin;
        else
            y += notification->height + context->bubble_margin;
    }
}

static void
_eventd_nd_event_updated(EventdEvent *event, EventdNdNotification *old_notification)
{
    EventdPluginContext *context = old_notification->context;
    EventdNdStyle *style = old_notification->style;

    EventdNdNotification *notification;
    notification = _eventd_nd_notification_new(context, event, style);

    notification->notification = old_notification->notification;
    notification->notification->data = notification;

    _eventd_nd_notification_free(old_notification);

    _eventd_nd_update_notifications(context);
}

static void
_eventd_nd_event_ended(EventdEvent *event, EventdEventEndReason reason, EventdNdNotification *notification)
{
    EventdPluginContext *context = notification->context;

    g_queue_delete_link(context->queue, notification->notification);
    _eventd_nd_notification_free(notification);

    _eventd_nd_update_notifications(context);
}

static void
_eventd_nd_event_action(EventdPluginContext *context, const gchar *config_id, EventdEvent *event)
{
    EventdNdStyle *style;

    if ( context->displays == NULL )
        return;

    style = g_hash_table_lookup(context->events, config_id);
    if ( style == NULL )
        return;

    EventdNdNotification *notification;
    notification = _eventd_nd_notification_new(context, event, style);

    g_signal_connect(event, "updated", G_CALLBACK(_eventd_nd_event_updated), notification);
    g_signal_connect(event, "ended", G_CALLBACK(_eventd_nd_event_ended), notification);

    if ( context->bubble_anchor & EVENTD_ND_ANCHOR_REVERSE )
    {
        g_queue_push_tail(context->queue, notification);
        notification->notification = g_queue_peek_tail_link(context->queue);
    }
    else
    {
        g_queue_push_head(context->queue, notification);
        notification->notification = g_queue_peek_head_link(context->queue);
    }

    _eventd_nd_update_notifications(context);
}


/*
 * Plugin interface
 */

EVENTD_EXPORT const gchar *eventd_plugin_id = "eventd-nd";
EVENTD_EXPORT
void
eventd_plugin_get_interface(EventdPluginInterface *interface)
{
    interface->init   = _eventd_nd_init;
    interface->uninit = _eventd_nd_uninit;

    interface->control_command = _eventd_nd_control_command;

    interface->global_parse = _eventd_nd_global_parse;
    interface->event_parse  = _eventd_nd_event_parse;
    interface->config_reset = _eventd_nd_config_reset;

    interface->event_action = _eventd_nd_event_action;
}

