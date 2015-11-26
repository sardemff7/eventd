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

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <glib.h>
#include <glib-object.h>

#include <pango/pango.h>
#include <cairo.h>

#include <eventd-plugin.h>
#include <libeventd-event.h>
#include <libeventd-helpers-config.h>

#include <eventd-nd-backend.h>

#include "backends.h"
#include "style.h"
#include "cairo.h"


struct _EventdPluginContext {
    EventdNdInterface interface;
    GSList *actions;
    gint max_width;
    gint max_height;
    EventdNdStyle *style;
    GHashTable *backends;
    GHashTable *displays;
    GQueue *queue;
};

typedef struct {
    EventdNdBackend *backend;
    EventdNdDisplay *display;
} EventdNdDisplayContext;

typedef struct {
    EventdNdBackend *backend;
    EventdNdDisplay *display;
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
_eventd_nd_backend_remove_display(EventdNdContext *context, const gchar *target)
{
    g_hash_table_remove(context->displays, target);
}

/*
 * Initialization interface
 */

static EventdPluginContext *
_eventd_nd_init(EventdPluginCoreContext *core, EventdPluginCoreInterface *interface)
{
    EventdPluginContext *context;

    context = g_new0(EventdPluginContext, 1);

    context->interface.remove_display = _eventd_nd_backend_remove_display;

    context->style = eventd_nd_style_new(NULL);

    context->backends = eventd_nd_backends_load(context, &context->interface);
    context->displays = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, _eventd_nd_backend_display_free);

    context->queue = g_queue_new();

    eventd_nd_cairo_init();

    return context;
}

static void
_eventd_nd_uninit(EventdPluginContext *context)
{
    eventd_nd_style_free(context->style);

    eventd_nd_cairo_uninit();

    g_queue_free(context->queue);

    g_hash_table_unref(context->displays);
    g_hash_table_unref(context->backends);

    g_free(context);
}


/*
 * Start/stop interface
 */

static void
_eventd_nd_start(EventdPluginContext *context)
{
    EventdNdDisplay *display;
    GHashTableIter iter;
    const gchar *id;
    EventdNdBackend *backend;
    const gchar *target;

    g_hash_table_iter_init(&iter, context->backends);
    while ( g_hash_table_iter_next(&iter, (gpointer *)&id, (gpointer *)&backend) )
    {
        target = backend->default_target(backend->context);
        if ( target == NULL )
            continue;

        display = backend->display_new(backend->context, target);
        if ( display == NULL )
            continue;

        EventdNdDisplayContext *display_context;

        display_context = g_new(EventdNdDisplayContext, 1);
        display_context->backend = backend;
        display_context->display = display;

        g_hash_table_insert(context->displays, g_strdup(target), display_context);
    }
}


/*
 * Control command interface
 */

static EventdPluginCommandStatus
_eventd_nd_control_command(EventdPluginContext *context, guint64 argc, const gchar * const *argv, gchar **status)
{
    EventdPluginCommandStatus r;
    GHashTableIter iter;
    const gchar *id;
    EventdNdBackend *backend;
    EventdNdDisplay *display;

    if ( g_strcmp0(argv[0], "attach") == 0 )
    {
        if ( argc < 2 )
        {
            *status = g_strdup("No target specified");
            r = EVENTD_PLUGIN_COMMAND_STATUS_COMMAND_ERROR;
        }
        else
        {
            const gchar *attached = NULL;

            g_hash_table_iter_init(&iter, context->backends);
            while ( ( attached == NULL ) && g_hash_table_iter_next(&iter, (gpointer *)&id, (gpointer *)&backend) )
            {
                display = backend->display_new(backend->context, argv[1]);
                if ( display == NULL )
                    continue;

                EventdNdDisplayContext *display_context;
                display_context = g_new(EventdNdDisplayContext, 1);
                display_context->backend = backend;
                display_context->display = display;

                g_hash_table_insert(context->displays, g_strdup(argv[1]), display_context);

                attached = id;
            }
            if ( attached != NULL )
            {
                *status = g_strdup_printf("Backend %s attached to %s", attached, argv[1]);
                r = EVENTD_PLUGIN_COMMAND_STATUS_OK;
            }
            else
            {
                *status = g_strdup("No backend attached");
                r = EVENTD_PLUGIN_COMMAND_STATUS_EXEC_ERROR;
            }
        }
    }
    else if ( g_strcmp0(argv[0], "detach") == 0 )
    {
        EventdNdDisplayContext *display_context;
        if ( argc < 2 )
        {
            r = EVENTD_PLUGIN_COMMAND_STATUS_COMMAND_ERROR;
            *status = g_strdup("No target specified");
        }
        else if ( ( display_context = g_hash_table_lookup(context->displays, argv[1]) ) != NULL )
        {
            EventdNdBackend *backend = display_context->backend;
            g_hash_table_remove(context->displays, argv[1]);
            *status = g_strdup_printf("Backend %s detached from %s", backend->id, argv[1]);
            r = EVENTD_PLUGIN_COMMAND_STATUS_OK;
        }
        else
        {
            *status = g_strdup("No backend detached");
            r = EVENTD_PLUGIN_COMMAND_STATUS_EXEC_ERROR;
        }
    }
    else if ( g_strcmp0(argv[0], "status") == 0 )
    {
        if ( g_hash_table_size(context->displays) > 0 )
        {
            GString *list;
            list = g_string_new("Backends attached:");

            GHashTableIter iter;
            gchar *target;
            EventdNdDisplayContext *display_context;
            g_hash_table_iter_init(&iter, context->displays);
            while ( g_hash_table_iter_next(&iter, (gpointer *)&target, (gpointer *)&display_context) )
                    g_string_append_printf(list, "\n    %s to %s", display_context->backend->id, target);

            *status = g_string_free(list, FALSE);
        }
        else
            *status = g_strdup("No backend attached");
        r = EVENTD_PLUGIN_COMMAND_STATUS_OK;
    }
    else if ( g_strcmp0(argv[0], "backends") == 0 )
    {
        if ( g_hash_table_size(context->backends) > 0 )
        {
            GString *list;
            list = g_string_new("Backends available:");

            GHashTableIter iter;
            EventdNdBackend *backend;
            g_hash_table_iter_init(&iter, context->backends);
            while ( g_hash_table_iter_next(&iter, NULL, (gpointer *)&backend) )
                    g_string_append_printf(list, "\n    %s", backend->id);

            *status = g_string_free(list, FALSE);
        }
        else
            *status = g_strdup("No backends available");
        r = EVENTD_PLUGIN_COMMAND_STATUS_OK;
    }
    else
    {
        *status = g_strdup_printf("Unknown command '%s'", argv[0]);
        r = EVENTD_PLUGIN_COMMAND_STATUS_COMMAND_ERROR;
    }

    return r;
}


/*
 * Configuration interface
 */

static void
_eventd_nd_global_parse(EventdPluginContext *context, GKeyFile *config_file)
{
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

static EventdPluginAction *
_eventd_nd_action_parse(EventdPluginContext *context, GKeyFile *config_file)
{
    gboolean disable;

    if ( ! g_key_file_has_group(config_file, "Notification") )
        return NULL;

    if ( evhelpers_config_key_file_get_boolean(config_file, "Notification", "Disable", &disable) < 0 )
        return NULL;

    if ( disable )
        return NULL;

    EventdNdStyle *style;
    style = eventd_nd_style_new(context->style);
    eventd_nd_style_update(style, config_file, &context->max_width, &context->max_height);

    context->actions = g_slist_prepend(context->actions, style);
    return style;
}

static void
_eventd_nd_config_reset(EventdPluginContext *context)
{
    g_slist_free_full(context->actions, eventd_nd_style_free);
    context->actions = NULL;
}


/*
 * Event action interface
 */

static void
_eventd_nd_notification_set(EventdNdNotification *self, EventdPluginContext *context, EventdEvent *event, cairo_surface_t **bubble)
{
    EventdNdNotificationContents *notification;

    notification = eventd_nd_notification_contents_new(self->style, event, context->max_width, context->max_height);
    *bubble = eventd_nd_cairo_get_surface(event, notification, self->style);
    eventd_nd_notification_contents_free(notification);

    self->width = cairo_image_surface_get_width(*bubble);
    self->height = cairo_image_surface_get_height(*bubble);
}

static EventdNdNotification *
_eventd_nd_notification_new(EventdPluginContext *context, EventdEvent *event, EventdNdStyle *style)
{
    EventdNdNotification *self;

    self = g_new0(EventdNdNotification, 1);
    self->context = context;
    self->style = style;

    cairo_surface_t *bubble;

    _eventd_nd_notification_set(self, context, event, &bubble);

    GHashTableIter iter;
    EventdNdDisplayContext *display;

    g_hash_table_iter_init(&iter, context->displays);
    while ( g_hash_table_iter_next(&iter, NULL, (gpointer *)&display) )
    {
        EventdNdSurfaceContext *surface;

        surface = g_new(EventdNdSurfaceContext, 1);
        surface->backend = display->backend;
        surface->display = display->display;
        surface->surface = display->backend->surface_new(event, display->display, bubble);

        self->surfaces = g_list_prepend(self->surfaces, surface);
    }

    cairo_surface_destroy(bubble);

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
_eventd_nd_notification_update(EventdNdNotification *self,EventdPluginContext *context,  EventdEvent *event)
{
    cairo_surface_t *bubble;

    _eventd_nd_notification_set(self, context, event, &bubble);

    GList *surface_;
    for ( surface_ = self->surfaces ; surface_ != NULL ; surface_ = g_list_next(surface_) )
    {
        EventdNdSurfaceContext *surface = surface_->data;
        if ( surface->backend->surface_update != NULL )
            surface->backend->surface_update(surface->surface, bubble);
        else
        {
            surface->backend->surface_free(surface->surface);
            surface->surface = surface->backend->surface_new(event, surface->display, bubble);
        }
    }

    cairo_surface_destroy(bubble);
}

/*
 * TODO: Rework notification update detection
 */
static void
_eventd_nd_event_updated(EventdEvent *event, EventdNdNotification *self)
{
    EventdPluginContext *context = self->context;
    _eventd_nd_notification_update(self, context, event);
}

static void
_eventd_nd_event_ended(EventdEvent *event, EventdEventEndReason reason, EventdNdNotification *notification)
{
    EventdPluginContext *context = notification->context;

    g_queue_delete_link(context->queue, notification->notification);
    _eventd_nd_notification_free(notification);
}

static void
_eventd_nd_event_action(EventdPluginContext *context, EventdNdStyle *style, EventdEvent *event)
{
    if ( g_hash_table_size(context->displays) == 0 )
        return;

    EventdNdNotification *notification;
    notification = _eventd_nd_notification_new(context, event, style);

    g_signal_connect(event, "ended", G_CALLBACK(_eventd_nd_event_ended), notification);

    g_queue_push_tail(context->queue, notification);
    notification->notification = g_queue_peek_tail_link(context->queue);
}


/*
 * Plugin interface
 */

EVENTD_EXPORT const gchar *eventd_plugin_id = "nd";
EVENTD_EXPORT
void
eventd_plugin_get_interface(EventdPluginInterface *interface)
{
    eventd_plugin_interface_add_init_callback(interface, _eventd_nd_init);
    eventd_plugin_interface_add_uninit_callback(interface, _eventd_nd_uninit);

    eventd_plugin_interface_add_start_callback(interface, _eventd_nd_start);

    eventd_plugin_interface_add_control_command_callback(interface, _eventd_nd_control_command);

    eventd_plugin_interface_add_global_parse_callback(interface, _eventd_nd_global_parse);
    eventd_plugin_interface_add_action_parse_callback(interface, _eventd_nd_action_parse);
    eventd_plugin_interface_add_config_reset_callback(interface, _eventd_nd_config_reset);

    eventd_plugin_interface_add_event_action_callback(interface, _eventd_nd_event_action);
}

