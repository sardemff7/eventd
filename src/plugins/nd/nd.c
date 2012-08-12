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
#include <glib-object.h>
#include <gio/gio.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo.h>

#include <eventd-plugin.h>
#include <libeventd-event.h>
#include <libeventd-config.h>
#include <libeventd-nd-notification.h>
#include <libeventd-nd-notification-template.h>

#include <eventd-nd-types.h>
#include <eventd-nd-backend.h>

#include "backends.h"
#include "style.h"
#include "cairo.h"

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
    GHashTable *surfaces;
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
    LibeventdNdNotificationTemplate *template;
    EventdNdStyle *style;
} EventdNdEvent;

static void
_eventd_nd_get_max_width_and_height(gint width, gint height, EventdNdStyle *style, gint *new_width, gint *new_height)
{
    gint w, h;

    w = eventd_nd_style_get_image_max_width(style);
    h = eventd_nd_style_get_image_max_height(style);

    width = MAX(width, w);
    height = MAX(height, h);

    w = eventd_nd_style_get_icon_max_width(style);
    h = eventd_nd_style_get_icon_max_height(style);

    width = MAX(width, w);
    height = MAX(height, h);

    *new_width = width;
    *new_height = height;
}

static EventdNdEvent *
_eventd_nd_event_new(EventdPluginContext *context, GKeyFile *config_file)
{
    EventdNdEvent *event = NULL;

    event = g_new0(EventdNdEvent, 1);

    event->template = libeventd_nd_notification_template_new(config_file);

    event->style = eventd_nd_style_new(context->style);
    eventd_nd_style_update(event->style, config_file);

    return event;
}

static void
_eventd_nd_event_free(gpointer data)
{
    EventdNdEvent *event = data;

    eventd_nd_style_free(event->style);
    libeventd_nd_notification_template_free(event->template);

    g_free(event);
}

static void
_eventd_nd_event_parse(EventdPluginContext *context, const gchar *id, GKeyFile *config_file)
{
    gboolean disable;
    EventdNdEvent *event = NULL;

    if ( ! g_key_file_has_group(config_file, "Notification") )
        return;

    if ( libeventd_config_key_file_get_boolean(config_file, "Notification", "Disable", &disable) < 0 )
        return;

    if ( ! disable )
    {
        event = _eventd_nd_event_new(context, config_file);
        _eventd_nd_get_max_width_and_height(context->max_width, context->max_height, event->style, &context->max_width, &context->max_height);
    }

    g_hash_table_insert(context->events, g_strdup(id), event);
}

static void
_eventd_nd_surface_hide(gpointer data)
{
    EventdNdSurfaceContext *surface = data;

    surface->backend->surface_hide(surface->surface);

    g_free(surface);
}

static void
_eventd_nd_surface_hide_all(gpointer data)
{
    GList *surfaces = data;

    g_list_free_full(surfaces, _eventd_nd_surface_hide);
}

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

static EventdPluginContext *
_eventd_nd_init(EventdCoreContext *core, EventdCoreInterface *interface)
{
    EventdPluginContext *context;

    context = g_new0(EventdPluginContext, 1);

    context->interface.remove_display = _eventd_nd_backend_remove_display;

    context->backends = eventd_nd_backends_load(context, &context->interface);

    context->events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, _eventd_nd_event_free);
    context->surfaces = g_hash_table_new_full(g_direct_hash, g_direct_equal, g_object_unref, _eventd_nd_surface_hide_all);

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

    g_hash_table_unref(context->surfaces);
    g_hash_table_unref(context->events);

    g_list_free_full(context->displays, _eventd_nd_backend_display_free);

    g_hash_table_unref(context->backends);

    g_free(context);
}

static void
_eventd_nd_control_command(EventdPluginContext *context, const gchar *command)
{
    const gchar *target = command+20;
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

        display = backend->display_new(backend->context, target, context->bubble_anchor, context->bubble_margin);
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

static void
_eventd_nd_global_parse(EventdPluginContext *context, GKeyFile *config_file)
{
    if ( g_key_file_has_group(config_file, "Notification") )
    {
        Int integer;
        gchar *string;

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

        if ( libeventd_config_key_file_get_int(config_file, "Notification", "Margin", &integer) == 0 )
            context->bubble_margin = integer.value;
    }

    eventd_nd_style_update(context->style, config_file);
    _eventd_nd_get_max_width_and_height(context->max_width, context->max_height, context->style, &context->max_width, &context->max_height);
}

static void
_eventd_nd_event_updated(EventdEvent *event, EventdPluginContext *context)
{
    EventdNdEvent *nd_event;

    nd_event = g_hash_table_lookup(context->events, eventd_event_get_config_id(event));
    if ( nd_event == NULL )
        return;

    LibeventdNdNotification *notification;
    cairo_surface_t *bubble;
    cairo_surface_t *shape;

    notification = libeventd_nd_notification_new(nd_event->template, event, context->max_width, context->max_height);
    eventd_nd_cairo_get_surfaces(event, notification, nd_event->style, &bubble, &shape);

    GList *surfaces = g_hash_table_lookup(context->surfaces, event);
    for ( ; surfaces != NULL ; surfaces = g_list_next(surfaces) )
    {
        EventdNdSurfaceContext *surface = surfaces->data;

        if ( surface->backend->surface_update != NULL )
            surface->surface = surface->backend->surface_update(surface->surface, bubble, shape);
    }

    cairo_surface_destroy(bubble);
    cairo_surface_destroy(shape);
}

static void
_eventd_nd_event_ended(EventdEvent *event, EventdEventEndReason reason, EventdPluginContext *context)
{
    g_hash_table_remove(context->surfaces, event);
}

static void
_eventd_nd_event_action(EventdPluginContext *context, EventdEvent *event)
{
    EventdNdEvent *nd_event;

    nd_event = g_hash_table_lookup(context->events, eventd_event_get_config_id(event));
    if ( nd_event == NULL )
        return;

    LibeventdNdNotification *notification;
    cairo_surface_t *bubble;
    cairo_surface_t *shape;

    notification = libeventd_nd_notification_new(nd_event->template, event, context->max_width, context->max_height);
    eventd_nd_cairo_get_surfaces(event, notification, nd_event->style, &bubble, &shape);

    GList *surfaces = NULL;

    GList *display_;
    for ( display_ = context->displays ; display_ != NULL ; display_ = g_list_next(display_) )
    {
        EventdNdDisplayContext *display = display_->data;
        EventdNdSurface *surface;
        surface = display->backend->surface_show(event, display->display, bubble, shape);
        if ( surface != NULL )
        {
            EventdNdSurfaceContext *surface_context;

            surface_context = g_new(EventdNdSurfaceContext, 1);
            surface_context->backend = display->backend;
            surface_context->surface = surface;

            surfaces = g_list_prepend(surfaces, surface_context);
        }
    }

    cairo_surface_destroy(bubble);
    cairo_surface_destroy(shape);

    if ( surfaces != NULL )
    {
        g_signal_connect(event, "updated", G_CALLBACK(_eventd_nd_event_updated), context);
        g_signal_connect(event, "ended", G_CALLBACK(_eventd_nd_event_ended), context);

        g_hash_table_insert(context->surfaces, g_object_ref(event), surfaces);
    }

    libeventd_nd_notification_free(notification);
}

static void
_eventd_nd_config_reset(EventdPluginContext *context)
{
    g_hash_table_remove_all(context->events);
}

EVENTD_EXPORT const gchar *eventd_plugin_id = "eventd-nd";
EVENTD_EXPORT
void
eventd_plugin_get_info(EventdPlugin *plugin)
{
    plugin->init = _eventd_nd_init;
    plugin->uninit = _eventd_nd_uninit;

    plugin->control_command = _eventd_nd_control_command;

    plugin->config_reset = _eventd_nd_config_reset;

    plugin->global_parse = _eventd_nd_global_parse;
    plugin->event_parse = _eventd_nd_event_parse;
    plugin->event_action = _eventd_nd_event_action;
}

