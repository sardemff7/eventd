/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2021 Quentin "Sardem FF7" Glidic
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

#include <string.h>

#include <glib.h>
#include <glib-object.h>

#include <pango/pango.h>
#include <cairo.h>

#include "eventd-plugin.h"
#include "libeventd-event.h"
#include "libeventd-helpers-config.h"

#include <nkutils-enum.h>

#include "wayland.h"
#include "style.h"
#include "cairo.h"
#include "notification.h"

#include "nd.h"

static const gchar * const _eventd_nd_anchors[_EVENTD_ND_ANCHOR_SIZE] = {
    [EVENTD_ND_ANCHOR_TOP_LEFT]     = "top-left",
    [EVENTD_ND_ANCHOR_TOP]          = "top",
    [EVENTD_ND_ANCHOR_TOP_RIGHT]    = "top-right",
    [EVENTD_ND_ANCHOR_BOTTOM_LEFT]  = "bottom-left",
    [EVENTD_ND_ANCHOR_BOTTOM]       = "bottom",
    [EVENTD_ND_ANCHOR_BOTTOM_RIGHT] = "bottom-right",
};

static const gchar * const _eventd_nd_dismiss_targets[] = {
    [EVENTD_ND_DISMISS_NONE]   = "none",
    [EVENTD_ND_DISMISS_ALL]    = "all",
    [EVENTD_ND_DISMISS_OLDEST] = "oldest",
    [EVENTD_ND_DISMISS_NEWEST] = "newest",
};

static const gchar * const  _eventd_nd_icon_fallback_themes[] = {
    "Adwaita",
    "gnome",
    NULL
};

void
eventd_nd_shaping_update(EventdNdContext *context, EventdNdShaping shaping)
{
    if ( context->shaping == shaping )
        return;

    context->shaping = shaping;

    eventd_nd_notification_refresh_list(context, TRUE);
}

void
eventd_nd_geometry_update(EventdNdContext *context, gint w, gint h, gint s)
{
    gboolean resize;
    resize = ( ( context->geometry.w != w ) || ( context->geometry.h != h ) || ( context->geometry.s != s ) );

    context->geometry.w = w;
    context->geometry.h = h;
    context->geometry.s = s;

    eventd_nd_notification_refresh_list(context, resize);
}

static EventdNdQueue *
_eventd_nd_queue_new(void)
{
    EventdNdQueue *self;

    self = g_new0(EventdNdQueue, 1);

    self->anchor = EVENTD_ND_ANCHOR_TOP_RIGHT;

    /* Defaults placement values */
    self->limit = 1;

    self->margin_x = 13;
    self->margin_y = 13;
    self->spacing = 13;

    self->wait_queue = g_queue_new();
    self->queue = g_queue_new();

    return self;
}

static void
_eventd_nd_queue_free(gpointer data)
{
    EventdNdQueue *self = data;

    g_queue_free(self->queue);
    g_queue_free(self->wait_queue);

    g_free(self);
}

static gboolean
_eventd_nd_bindings_always_trigger(guint64 scope, gpointer target, gpointer user_data)
{
    return NK_BINDINGS_BINDING_TRIGGERED;
}

static void
_eventd_nd_bindings_dismiss_callback(guint64 scope, gpointer target, gpointer user_data)
{
    EventdNdNotification *notification = target;

    eventd_nd_notification_dismiss(notification);
}

static void
_eventd_nd_bindings_dismiss_queue_callback(guint64 scope, gpointer target, gpointer user_data)
{
    EventdNdNotification *notification = target;

    eventd_nd_notification_dismiss_queue(notification);
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

    context->bindings = nk_bindings_new(-1);
    nk_bindings_add_binding(context->bindings, 0, "MousePrimary", _eventd_nd_bindings_always_trigger,_eventd_nd_bindings_dismiss_callback, context, NULL, NULL);
    nk_bindings_add_binding(context->bindings, 0, "MouseSecondary", _eventd_nd_bindings_always_trigger, _eventd_nd_bindings_dismiss_queue_callback, context, NULL, NULL);

    context->wayland = eventd_nd_wl_init(context, context->bindings);

    context->style = eventd_nd_style_new(NULL);

    context->queues = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, _eventd_nd_queue_free);

    g_hash_table_insert(context->queues, g_strdup("default"), _eventd_nd_queue_new());

    context->notifications = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, eventd_nd_notification_free);

    return context;
}

static void
_eventd_nd_uninit(EventdPluginContext *context)
{
    g_hash_table_unref(context->notifications);

    g_hash_table_unref(context->queues);

    eventd_nd_style_free(context->style);

    g_free(context->last_target);
    eventd_nd_wl_uninit(context->wayland);

    nk_bindings_free(context->bindings);

    g_free(context);
}


/*
 * Start/stop interface
 */

static void
_eventd_nd_start(EventdPluginContext *context)
{
    eventd_nd_wl_start(context->wayland, context->last_target);
    context->theme_context = nk_xdg_theme_context_new(_eventd_nd_icon_fallback_themes, NULL);
}

static void
_eventd_nd_stop(EventdPluginContext *context)
{
    nk_xdg_theme_context_free(context->theme_context);
    eventd_nd_wl_stop(context->wayland);
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
            *status = g_strdup("No target specified");
            r = EVENTD_PLUGIN_COMMAND_STATUS_COMMAND_ERROR;
        }
        else
        {
            if ( eventd_nd_wl_start(context->wayland, argv[2]) )
            {
                r = EVENTD_PLUGIN_COMMAND_STATUS_OK;
                g_free(context->last_target);
                context->last_target = g_strdup(argv[2]);
            }
            else
            {
                *status = g_strdup_printf("Could not connect to %s", argv[2]);
                r = EVENTD_PLUGIN_COMMAND_STATUS_EXEC_ERROR;
            }
        }
    }
    else if ( g_strcmp0(argv[0], "stop") == 0 )
    {
        eventd_nd_wl_stop(context->wayland);
        r = EVENTD_PLUGIN_COMMAND_STATUS_OK;
    }
    else if ( g_strcmp0(argv[0], "dismiss") == 0 )
    {
        r = EVENTD_PLUGIN_COMMAND_STATUS_COMMAND_ERROR;
        if ( argc < 2 )
            *status = g_strdup("No dismiss target specified");
        else
        {
            guint64 target = EVENTD_ND_DISMISS_NONE;
            nk_enum_parse(argv[1], _eventd_nd_dismiss_targets, G_N_ELEMENTS(_eventd_nd_dismiss_targets), NK_ENUM_MATCH_FLAGS_IGNORE_CASE, &target);

            if ( target != EVENTD_ND_DISMISS_NONE )
            {
                if ( argc > 2 )
                {
                    EventdNdQueue *queue;
                    queue = g_hash_table_lookup(context->queues, argv[2]);
                    if ( queue != NULL )
                        r = eventd_nd_notification_dismiss_target(context, target, queue);
                    else
                        *status = g_strdup_printf("Unknown queue '%s'", argv[2]);
                }
                else
                    r = eventd_nd_notification_dismiss_target(context, target, NULL);
            }
            else
                *status = g_strdup_printf("Unknown dismiss target '%s'", argv[1]);
        }
    }
    else if ( g_strcmp0(argv[0], "status") == 0 )
    {
        /* TODO: fix */
        {
            *status = g_strdup("No backend attached");
            r = EVENTD_PLUGIN_COMMAND_STATUS_OK;
        }
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
    eventd_nd_style_update(context->style, config_file);

    eventd_nd_wl_global_parse(context->wayland, config_file);

    if ( g_key_file_has_group(config_file, "NotificationBindings") )
    {
        gchar *string;
        GError *error = NULL;
        nk_bindings_reset_bindings(context->bindings);

        if ( evhelpers_config_key_file_get_string(config_file, "NotificationBindings", "Dismiss", &string) == 0 )
        {
            if ( ! nk_bindings_add_binding(context->bindings, 0, string, _eventd_nd_bindings_always_trigger, _eventd_nd_bindings_dismiss_callback, context, NULL, &error) )
            {
                g_warning("Could not add dismiss binding: %s", error->message);
                g_clear_error(&error);
            }
            g_free(string);
        }

        if ( evhelpers_config_key_file_get_string(config_file, "NotificationBindings", "DismissQueue", &string) == 0 )
        {
            if ( ! nk_bindings_add_binding(context->bindings, 0, string, _eventd_nd_bindings_always_trigger, _eventd_nd_bindings_dismiss_queue_callback, context, NULL, &error) )
            {
                g_warning("Could not add dismiss queue binding: %s", error->message);
                g_clear_error(&error);
            }
            g_free(string);
        }
    }

    gchar **groups, **group;
    groups = g_key_file_get_groups(config_file, NULL);
    if ( groups == NULL )
        return;

    for ( group = groups ; *group != NULL ; ++group )
    {
        if ( ! g_str_has_prefix(*group, "Queue ") )
            continue;

        const gchar *name = *group + strlen("Queue ");
        EventdNdQueue *self;

        self = g_hash_table_lookup(context->queues, name);
        if ( self == NULL )
        {
            self = _eventd_nd_queue_new();
            g_hash_table_insert(context->queues, g_strdup(name), self);
        }

        guint64 anchor;
        Int integer;
        Int integer_list[2];
        gsize length = 2;
        gboolean boolean;

        if ( evhelpers_config_key_file_get_enum(config_file, *group, "Anchor", _eventd_nd_anchors, G_N_ELEMENTS(_eventd_nd_anchors), &anchor) == 0 )
            self->anchor = anchor;

        if ( evhelpers_config_key_file_get_int(config_file, *group, "Limit", &integer) == 0 )
            self->limit = ( integer.value > 0 ) ? integer.value : 0;

        if ( evhelpers_config_key_file_get_boolean(config_file, *group, "MoreIndicator", &boolean) == 0 )
            self->more_indicator = boolean;

        if ( evhelpers_config_key_file_get_int_list(config_file, *group, "Margin", integer_list, &length) == 0 )
        {
            switch ( length )
            {
            case 1:
                integer_list[1] = integer_list[0];
                /* fallthrough */
            case 2:
                self->margin_x = MAX(0, integer_list[0].value);
                self->margin_y = MAX(0, integer_list[1].value);
            break;
            }
        }

        if ( evhelpers_config_key_file_get_int(config_file, *group, "Spacing", &integer) == 0 )
            self->spacing = ( integer.value > 0 ) ? integer.value : 0;

        if ( evhelpers_config_key_file_get_boolean(config_file, *group, "OldestAtAnchor", &boolean) == 0 )
            self->reverse = boolean;
    }
    g_strfreev(groups);
}

static EventdPluginAction *
_eventd_nd_action_parse(EventdPluginContext *context, GKeyFile *config_file)
{
    gboolean disable = FALSE;

    if ( ! g_key_file_has_group(config_file, "Notification") )
        return NULL;

    if ( evhelpers_config_key_file_get_boolean(config_file, "Notification", "Disable", &disable) < 0 )
        return NULL;

    if ( disable )
        return NULL;

    EventdNdStyle *style;
    style = eventd_nd_style_new(context->style);
    eventd_nd_style_update(style, config_file);

    context->actions = g_slist_prepend(context->actions, style);
    return style;
}

static void
_eventd_nd_config_reset(EventdPluginContext *context)
{
    eventd_nd_wl_config_reset(context->wayland);

    g_slist_free_full(context->actions, eventd_nd_style_free);
    context->actions = NULL;

    nk_bindings_reset_bindings(context->bindings);
    nk_bindings_add_binding(context->bindings, 0, "MousePrimary", _eventd_nd_bindings_always_trigger, _eventd_nd_bindings_dismiss_callback, context, NULL, NULL);
    nk_bindings_add_binding(context->bindings, 0, "MouseSecondary", _eventd_nd_bindings_always_trigger, _eventd_nd_bindings_dismiss_queue_callback, context, NULL, NULL);

    eventd_nd_style_free(context->style);
    context->style = eventd_nd_style_new(NULL);
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
    uuid = eventd_event_get_data_string(event, "source-event");
    if ( ( uuid == NULL ) || ( ! g_hash_table_contains(context->notifications, uuid) ) )
        return;

    g_hash_table_remove(context->notifications, uuid);
}

static void
_eventd_nd_event_action(EventdPluginContext *context, EventdNdStyle *style, EventdEvent *event)
{
    /* TODO: add a connected check */
    if ( FALSE )
        /* No backend connected for now */
        return;

    const gchar *uuid;
    EventdNdNotification *notification;
    GVariant *end;

    uuid = eventd_event_get_uuid(event);

    end = eventd_event_get_data(event, ".event-end");
    if ( ( end != NULL ) && g_variant_is_of_type(end, G_VARIANT_TYPE_BOOLEAN) && g_variant_get_boolean(end) )
    {
        /*
         * This is an update event, so it take the same way as the
         * original event, and it is safe to just drop our bubble
         * without propagating a ".notification" event.
         */
        if ( g_hash_table_contains(context->notifications, uuid) )
            g_hash_table_remove(context->notifications, uuid);
        return;
    }

    notification = g_hash_table_lookup(context->notifications, uuid);

    if ( notification == NULL )
    {
        notification = eventd_nd_notification_new(context, event, style);
        g_hash_table_insert(context->notifications, (gpointer) uuid, notification);
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

