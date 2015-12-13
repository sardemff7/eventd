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

#include "backend.h"
#include "style.h"
#include "cairo.h"

#include "backend-xcb.h"
#include "backend-fbdev.h"

typedef struct {
    EventdNdBackendContext *context;

    EventdNdBackendContext *(*init)(EventdNdContext *context);
    void (*uninit)(EventdNdBackendContext *context);

    void (*global_parse)(EventdNdBackendContext *context, GKeyFile *config_file);

    gboolean (*start)(EventdNdBackendContext *context, const gchar *target);
    void (*stop)(EventdNdBackendContext *context);

    EventdNdSurface *(*surface_new)(EventdNdBackendContext *context, EventdEvent *event, cairo_surface_t *bubble);
    void (*surface_update)(EventdNdSurface *surface, cairo_surface_t *bubble);
    void (*surface_free)(EventdNdSurface *surface);
} EventdNdBackend;

struct _EventdPluginContext {
    EventdPluginCoreContext *core;
    EventdPluginCoreInterface *core_interface;
    GSList *actions;
    gint max_width;
    gint max_height;
    EventdNdStyle *style;
    EventdNdBackend *backend;
    EventdNdBackend backends[_EVENTD_ND_BACKENDS_SIZE];
    GHashTable *notifications;
};

typedef struct {
    EventdNdContext *context;
    EventdNdStyle *style;
    EventdEvent *event;
    GList *notification;
    gint width;
    gint height;
    guint timeout;
    EventdNdSurface *surface;
} EventdNdNotification;


static const gchar *_eventd_nd_backends_names[_EVENTD_ND_BACKENDS_SIZE] = {
    [EVENTD_ND_BACKEND_NONE] = "none",
#ifdef ENABLE_ND_XCB
    [EVENTD_ND_BACKEND_XCB] = "xcb",
#endif /* ENABLE_ND_XCB */
#ifdef ENABLE_ND_FBDEV
    [EVENTD_ND_BACKEND_FBDEV] = "fbdev",
#endif /* ENABLE_ND_FBDEV */
};

void
eventd_nd_surface_remove(EventdNdContext *context, const gchar *uuid)
{
    EventdEvent *event;
    event = eventd_event_new(".notification", "dismiss");
    eventd_event_add_data(event, g_strdup("source-event"), g_strdup(uuid));
    eventd_plugin_core_push_event(context->core, context->core_interface, event);
    eventd_event_unref(event);
}

gboolean
eventd_nd_backend_switch(EventdNdContext *context, EventdNdBackends backend, const gchar *target)
{
    if ( context->backend != NULL )
    {
        context->backend->uninit(context->backend->context);
        context->backend = NULL;
    }

    if ( backend == EVENTD_ND_BACKEND_NONE )
        return TRUE;

    if ( ! context->backends[backend].start(context->backends[backend].context, target) )
        return FALSE;

    context->backend = &context->backends[backend];
    return TRUE;
}


static void
_eventd_nd_notification_free(gpointer data)
{
    EventdNdNotification *self = data;

    if ( self->timeout > 0 )
        g_source_remove(self->timeout);

    self->context->backend->surface_free(self->surface);

    g_free(self);
}


/*
 * Initialization interface
 */

#define _eventd_nd_add_backend_func(i, prefix, func) context->backends[i].func = prefix##func
#define _eventd_nd_add_backend(i, prefix) G_STMT_START { \
        _eventd_nd_add_backend_func(i, prefix, init); \
        _eventd_nd_add_backend_func(i, prefix, uninit); \
        _eventd_nd_add_backend_func(i, prefix, global_parse); \
        _eventd_nd_add_backend_func(i, prefix, start); \
        _eventd_nd_add_backend_func(i, prefix, stop); \
        _eventd_nd_add_backend_func(i, prefix, surface_new); \
        _eventd_nd_add_backend_func(i, prefix, surface_update); \
        _eventd_nd_add_backend_func(i, prefix, surface_free); \
    } G_STMT_END
#define eventd_nd_add_backend(name, Name) _eventd_nd_add_backend(EVENTD_ND_BACKEND_##Name, eventd_nd_##name##_)

static EventdPluginContext *
_eventd_nd_init(EventdPluginCoreContext *core, EventdPluginCoreInterface *interface)
{
    EventdPluginContext *context;

    context = g_new0(EventdPluginContext, 1);
    context->core = core;
    context->core_interface = interface;

    context->style = eventd_nd_style_new(NULL);

    context->notifications = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, _eventd_nd_notification_free);

    eventd_nd_cairo_init();

#ifdef ENABLE_ND_XCB
    eventd_nd_add_backend(xcb, XCB);
#endif /* ENABLE_ND_XCB */
#ifdef ENABLE_ND_FBDEV
    eventd_nd_add_backend(linux, FBDEV);
#endif /* ENABLE_ND_FBDEV */

    EventdNdBackends i;
    for ( i = EVENTD_ND_BACKEND_NONE + 1 ; i < _EVENTD_ND_BACKENDS_SIZE ; ++i )
        context->backends[i].context = context->backends[i].init(context);

    return context;
}

static void
_eventd_nd_uninit(EventdPluginContext *context)
{
    eventd_nd_style_free(context->style);

    EventdNdBackends i;
    for ( i = EVENTD_ND_BACKEND_NONE + 1 ; i < _EVENTD_ND_BACKENDS_SIZE ; ++i )
        context->backends[i].uninit(context->backends[i].context);

    eventd_nd_cairo_uninit();

    g_hash_table_unref(context->notifications);

    g_free(context);
}


/*
 * Start/stop interface
 */

static void
_eventd_nd_start(EventdPluginContext *context)
{
    EventdNdBackends backend = EVENTD_ND_BACKEND_NONE;
    const gchar *target = NULL;

#ifdef ENABLE_ND_XCB
    if ( backend == EVENTD_ND_BACKEND_NONE )
    {
        target = g_getenv("DISPLAY");
        if ( target != NULL )
            backend = EVENTD_ND_BACKEND_XCB;
    }
#endif /* ENABLE_ND_XCB */

#ifdef ENABLE_ND_FBDEV
    if ( backend == EVENTD_ND_BACKEND_NONE )
    {
        target = g_getenv("TTY");
        if ( ( target != NULL ) && g_str_has_prefix(target, "/dev/tty") )
        {
            backend = EVENTD_ND_BACKEND_FBDEV;
            target = "/dev/fb0";
        }
    }
#endif /* ENABLE_ND_FBDEV */

    eventd_nd_backend_switch(context, backend, target);
}

static void
_eventd_nd_stop(EventdPluginContext *context)
{
    eventd_nd_backend_switch(context, EVENTD_ND_BACKEND_NONE, NULL);
}


/*
 * Control command interface
 */

static EventdPluginCommandStatus
_eventd_nd_control_command(EventdPluginContext *context, guint64 argc, const gchar * const *argv, gchar **status)
{
    EventdPluginCommandStatus r;

    if ( g_strcmp0(argv[0], "switch") == 0 )
    {
        if ( argc < 2 )
        {
            *status = g_strdup("No backend specified");
            r = EVENTD_PLUGIN_COMMAND_STATUS_COMMAND_ERROR;
        }
        else if ( argc < 3 )
        {
            *status = g_strdup("No target specified");
            r = EVENTD_PLUGIN_COMMAND_STATUS_COMMAND_ERROR;
        }
        else
        {
            /* FIXME: parse argv[1] */
            eventd_nd_backend_switch(context, EVENTD_ND_BACKEND_NONE, argv[2]);
            r = EVENTD_PLUGIN_COMMAND_STATUS_OK;
        }
    }
    else if ( g_strcmp0(argv[0], "backends") == 0 )
    {
        GString *list;
        list = g_string_new("Backends available:");

        EventdNdBackends i;
        for ( i = EVENTD_ND_BACKEND_NONE + 1 ; i < _EVENTD_ND_BACKENDS_SIZE ; ++i )
                g_string_append_printf(list, "\n    %s", _eventd_nd_backends_names[i]);

        *status = g_string_free(list, FALSE);
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

static gboolean
_eventd_nd_event_timedout(gpointer user_data)
{
    EventdNdNotification *self = user_data;
    EventdPluginContext *context = self->context;
    EventdEvent *event;

    self->timeout = 0;
    event = eventd_event_new(".notification", "timeout");
    eventd_event_add_data(event, g_strdup("source-event"), g_strdup(eventd_event_get_uuid(self->event)));
    eventd_plugin_core_push_event(context->core, context->core_interface, event);
    eventd_event_unref(event);

    return FALSE;
}

static void
_eventd_nd_notification_set(EventdNdNotification *self, EventdPluginContext *context, EventdEvent *event, cairo_surface_t **bubble)
{
    eventd_event_unref(self->event);
    self->event = eventd_event_ref(event);

    EventdNdNotificationContents *notification;

    notification = eventd_nd_notification_contents_new(self->style, event, context->max_width, context->max_height);
    *bubble = eventd_nd_cairo_get_surface(event, notification, self->style);
    eventd_nd_notification_contents_free(notification);

    self->width = cairo_image_surface_get_width(*bubble);
    self->height = cairo_image_surface_get_height(*bubble);

    if ( self->timeout > 0 )
        g_source_remove(self->timeout);
    self->timeout = g_timeout_add_full(G_PRIORITY_DEFAULT, eventd_nd_style_get_bubble_timeout(self->style), _eventd_nd_event_timedout, self, NULL);
}

static EventdNdNotification *
_eventd_nd_notification_new(EventdPluginContext *context, EventdEvent *event, EventdNdStyle *style)
{
    EventdNdNotification *self;

    self = g_new0(EventdNdNotification, 1);
    self->context = context;
    self->style = style;
    self->event = eventd_event_ref(event);

    cairo_surface_t *bubble;

    _eventd_nd_notification_set(self, context, event, &bubble);

    self->surface = context->backend->surface_new(context->backend->context, event, bubble);

    cairo_surface_destroy(bubble);

    return self;
}

static void
_eventd_nd_notification_update(EventdNdNotification *self, EventdPluginContext *context,  EventdEvent *event)
{
    cairo_surface_t *bubble;

    _eventd_nd_notification_set(self, context, event, &bubble);

    context->backend->surface_update(self->surface, bubble);

    cairo_surface_destroy(bubble);
}

static void
_eventd_nd_event_dispatch(EventdPluginContext *context, EventdEvent *event)
{
    const gchar *category;
    category = eventd_event_get_category(event);
    if ( g_strcmp0(category, ".notification") != 0 )
        return;

    const gchar *uuid;
    uuid = eventd_event_get_data(event, "source-event");
    if ( ( uuid == NULL ) || ( ! g_hash_table_contains(context->notifications, uuid) ) )
        return;

    g_hash_table_remove(context->notifications, uuid);
}

static void
_eventd_nd_event_action(EventdPluginContext *context, EventdNdStyle *style, EventdEvent *event)
{
    if ( context->backend == EVENTD_ND_BACKEND_NONE )
        /* No backend connected for now */
        return;

    EventdNdNotification *notification;
    notification = g_hash_table_lookup(context->notifications, eventd_event_get_uuid(event));

    if ( notification == NULL )
    {
        notification = _eventd_nd_notification_new(context, event, style);
        g_hash_table_insert(context->notifications, (gpointer) eventd_event_get_uuid(event), notification);
    }
    else
        _eventd_nd_notification_update(notification, context, event);
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
    eventd_plugin_interface_add_stop_callback(interface, _eventd_nd_stop);

    eventd_plugin_interface_add_control_command_callback(interface, _eventd_nd_control_command);

    eventd_plugin_interface_add_global_parse_callback(interface, _eventd_nd_global_parse);
    eventd_plugin_interface_add_action_parse_callback(interface, _eventd_nd_action_parse);
    eventd_plugin_interface_add_config_reset_callback(interface, _eventd_nd_config_reset);

    eventd_plugin_interface_add_event_dispatch_callback(interface, _eventd_nd_event_dispatch);
    eventd_plugin_interface_add_event_action_callback(interface, _eventd_nd_event_action);
}

