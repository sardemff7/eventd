/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2016 Quentin "Sardem FF7" Glidic
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

#include <nkutils-enum.h>

#include "backend.h"
#include "backends.h"
#include "style.h"
#include "cairo.h"
#include "notification.h"

#include "nd.h"

const gchar *eventd_nd_backends_names[_EVENTD_ND_BACKENDS_SIZE] = {
    [EVENTD_ND_BACKEND_NONE] = "none",
#ifdef ENABLE_ND_XCB
    [EVENTD_ND_BACKEND_XCB] = "xcb",
#endif /* ENABLE_ND_XCB */
#ifdef ENABLE_ND_FBDEV
    [EVENTD_ND_BACKEND_FBDEV] = "fbdev",
#endif /* ENABLE_ND_FBDEV */
#ifdef ENABLE_ND_WIN
    [EVENTD_ND_BACKEND_WIN] = "win",
#endif /* ENABLE_ND_WIN */
};

static const gchar * const _eventd_nd_dismiss_targets[] = {
    [EVENTD_ND_DISMISS_NONE]   = "none",
    [EVENTD_ND_DISMISS_ALL]    = "all",
    [EVENTD_ND_DISMISS_OLDEST] = "oldest",
    [EVENTD_ND_DISMISS_NEWEST] = "newest",
};

static gboolean
_eventd_nd_backend_switch(EventdNdContext *context, EventdNdBackends backend, const gchar *target)
{
    if ( context->backend != NULL )
    {
        context->no_refresh = TRUE;
        g_hash_table_remove_all(context->notifications);
        context->no_refresh = FALSE;
        if ( context->backend->stop != NULL )
            context->backend->stop(context->backend->context);
        context->backend = NULL;
    }

    if ( backend == EVENTD_ND_BACKEND_NONE )
        return TRUE;

    EventdNdBackend *backend_;
    backend_ = &context->backends[backend];
    if ( backend_->context == NULL )
        return FALSE;

    if ( ( backend_->start != NULL ) && ( ! backend_->start(backend_->context, target) ) )
        return FALSE;

    context->backend = backend_;
    return TRUE;
}

static void
_eventd_nd_geometry_update(EventdNdContext *context, gint w, gint h)
{
    gboolean resize;
    resize = ( ( context->geometry.w != w ) || ( context->geometry.h != h ) );

    context->geometry.w = w;
    context->geometry.h = h;

    if ( context->backend == NULL )
        /* Start phase, nothing to update */
        return;

    eventd_nd_notification_geometry_changed(context, resize);
}

static gboolean
_eventd_nd_backend_stop(EventdNdContext *context)
{
    return _eventd_nd_backend_switch(context, EVENTD_ND_BACKEND_NONE, NULL);
}

/*
 * Initialization interface
 */

static EventdPluginContext *
_eventd_nd_init(EventdPluginCoreContext *core)
{
    EventdPluginContext *context;

    context = g_new0(EventdPluginContext, 1);
    context->core = core;

    context->interface.context = context;
    context->interface.geometry_update = _eventd_nd_geometry_update;
    context->interface.backend_stop = _eventd_nd_backend_stop;
    context->interface.notification_draw = eventd_nd_notification_draw;
    context->interface.notification_dismiss = eventd_nd_notification_dismiss;

    if ( ! eventd_nd_backends_load(context->backends, &context->interface) )
    {
        g_warning("Could not load any backend, aborting");
        g_free(context);
        return NULL;
    }

    context->style = eventd_nd_style_new(NULL);
    context->max_width = -1;
    context->max_height = -1;

    EventdNdAnchor i;
    for ( i = EVENTD_ND_ANCHOR_TOP_LEFT ; i < _EVENTD_ND_ANCHOR_SIZE ; ++i )
    {
        context->queues[i].anchor = i;

        /* Defaults placement values */
        context->queues[i].limit = 1;

        context->queues[i].margin = 13;
        context->queues[i].spacing = 13;

        context->queues[i].wait_queue = g_queue_new();
        context->queues[i].queue = g_queue_new();
    }

    context->notifications = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, eventd_nd_notification_free);

    return context;
}

static void
_eventd_nd_uninit(EventdPluginContext *context)
{
    g_hash_table_unref(context->notifications);

    EventdNdAnchor i;
    for ( i = EVENTD_ND_ANCHOR_TOP_LEFT ; i < _EVENTD_ND_ANCHOR_SIZE ; ++i )
    {
        g_queue_free(context->queues[i].queue);
        g_queue_free(context->queues[i].wait_queue);
    }

    eventd_nd_style_free(context->style);

    eventd_nd_backends_unload(context->backends);

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

#ifdef ENABLE_ND_WIN
#ifdef G_OS_WIN32
    if ( backend == EVENTD_ND_BACKEND_NONE )
    {
        backend = EVENTD_ND_BACKEND_WIN;
        target = "dummy";
    }
#endif /* G_OS_WIN32 */
#endif /* ENABLE_ND_WIN */

    _eventd_nd_backend_switch(context, backend, target);
}

static void
_eventd_nd_stop(EventdPluginContext *context)
{
    _eventd_nd_backend_stop(context);
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
        else
        {
            guint64 backend = EVENTD_ND_BACKEND_NONE;
            nk_enum_parse(argv[1], eventd_nd_backends_names, _EVENTD_ND_BACKENDS_SIZE, TRUE, &backend);

            if ( backend == EVENTD_ND_BACKEND_NONE )
            {
                *status = g_strdup_printf("Unknown backend %s", argv[1]);
                r = EVENTD_PLUGIN_COMMAND_STATUS_COMMAND_ERROR;
            }
            else if ( argc < 3 )
            {
                *status = g_strdup("No target specified");
                r = EVENTD_PLUGIN_COMMAND_STATUS_COMMAND_ERROR;
            }
            else
            {
                _eventd_nd_backend_switch(context, backend, argv[2]);
                r = EVENTD_PLUGIN_COMMAND_STATUS_OK;
            }
        }
    }
    else if ( g_strcmp0(argv[0], "stop") == 0 )
    {
        _eventd_nd_backend_stop(context);
        r = EVENTD_PLUGIN_COMMAND_STATUS_OK;
    }
    else if ( g_strcmp0(argv[0], "dismiss") == 0 )
    {
        r = EVENTD_PLUGIN_COMMAND_STATUS_OK;
        if ( argc < 2 )
        {
            *status = g_strdup("No dismiss target specified");
            r = EVENTD_PLUGIN_COMMAND_STATUS_COMMAND_ERROR;
        }
        else
        {
            guint64 target = EVENTD_ND_DISMISS_NONE;
            guint64 anchor = _EVENTD_ND_ANCHOR_SIZE;
            nk_enum_parse(argv[1], _eventd_nd_dismiss_targets, G_N_ELEMENTS(_eventd_nd_dismiss_targets), TRUE, &target);
            if ( argc > 2 )
                nk_enum_parse(argv[2], eventd_nd_anchors, _EVENTD_ND_ANCHOR_SIZE, TRUE, &anchor);

            if ( target != EVENTD_ND_DISMISS_NONE )
                eventd_nd_notification_dismiss_target(context, target, anchor);
            else
            {
                *status = g_strdup_printf("Unknown dismiss target '%s'", argv[1]);
                r = EVENTD_PLUGIN_COMMAND_STATUS_COMMAND_ERROR;
            }
        }
    }
    else if ( g_strcmp0(argv[0], "backends") == 0 )
    {
        GString *list;
        list = g_string_new("Backends available:");

        EventdNdBackends i;
        for ( i = EVENTD_ND_BACKEND_NONE + 1 ; i < _EVENTD_ND_BACKENDS_SIZE ; ++i )
        {
            if ( context->backends[i].context != NULL )
                g_string_append_printf(list, "\n    %s", eventd_nd_backends_names[i]);
        }

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

    EventdNdBackends i;
    for ( i = EVENTD_ND_BACKEND_NONE + 1 ; i < _EVENTD_ND_BACKENDS_SIZE ; ++i )
    {
        if ( ( context->backends[i].context != NULL ) && ( context->backends[i].global_parse != NULL ) )
            context->backends[i].global_parse(context->backends[i].context, config_file);
    }

    gsize anchor_name_length = strlen("Anchor bottom-right") + 1;
    gchar anchor_name[anchor_name_length];
    EventdNdAnchor j;
    for ( j = EVENTD_ND_ANCHOR_TOP_LEFT ; j < _EVENTD_ND_ANCHOR_SIZE ; ++j )
    {
        g_snprintf(anchor_name, anchor_name_length, "Anchor %s", eventd_nd_anchors[j]);
        if ( ! g_key_file_has_group(config_file, anchor_name) )
            continue;

        Int integer;
        gboolean boolean;

        if ( evhelpers_config_key_file_get_int(config_file, anchor_name, "Limit", &integer) == 0 )
            context->queues[j].limit = ( integer.value > 0 ) ? integer.value : 0;

        if ( evhelpers_config_key_file_get_boolean(config_file, anchor_name, "MoreIndicator", &boolean) == 0 )
            context->queues[j].more_indicator = boolean;

        if ( evhelpers_config_key_file_get_int(config_file, anchor_name, "Margin", &integer) == 0 )
            context->queues[j].margin = ( integer.value > 0 ) ? integer.value : 0;

        if ( evhelpers_config_key_file_get_int(config_file, anchor_name, "Spacing", &integer) == 0 )
            context->queues[j].spacing = ( integer.value > 0 ) ? integer.value : 0;

        if ( evhelpers_config_key_file_get_boolean(config_file, anchor_name, "OldestAtAnchor", &boolean) == 0 )
            context->queues[j].reverse = boolean;
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
    EventdNdBackends i;
    for ( i = EVENTD_ND_BACKEND_NONE + 1 ; i < _EVENTD_ND_BACKENDS_SIZE ; ++i )
    {
        if ( ( context->backends[i].context != NULL ) && ( context->backends[i].config_reset != NULL ) )
            context->backends[i].config_reset(context->backends[i].context);
    }

    g_slist_free_full(context->actions, eventd_nd_style_free);
    context->actions = NULL;
}


/*
 * Event action interface
 */

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
    if ( context->backend == NULL )
        /* No backend connected for now */
        return;

    EventdNdNotification *notification;
    notification = g_hash_table_lookup(context->notifications, eventd_event_get_uuid(event));

    if ( notification == NULL )
    {
        notification = eventd_nd_notification_new(context, event, style);
        g_hash_table_insert(context->notifications, (gpointer) eventd_event_get_uuid(event), notification);
    }
    else
        eventd_nd_notification_update(notification, event);
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

