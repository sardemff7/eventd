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

#include <eventd-plugin.h>
#include <libeventd-event.h>
#include <libeventd-config.h>

#include <eventd-nd-notification.h>

#include "types.h"
#include "daemon/daemon.h"

struct _EventdPluginContext {
    GHashTable *events;
    EventdNdContext *daemon;
    EventdNdStyleAnchor bubble_anchor;
    gint bubble_margin;
    EventdNdStyle *style;
    GHashTable *surfaces;
};

typedef struct {
    gchar *title;
    gchar *message;
    gchar *icon;
    gchar *overlay_icon;
    EventdNdStyle *style;
} EventdNdEvent;

static void
_eventd_nd_event_update(EventdNdEvent *event, GKeyFile *config_file)
{
    gchar *title = NULL;
    gchar *message = NULL;
    gchar *icon = NULL;
    gchar *overlay_icon = NULL;

    if ( libeventd_config_key_file_get_string(config_file, "notification", "title", &title) == 0 )
    {
        g_free(event->title);
        event->title = title;
    }
    if ( libeventd_config_key_file_get_string(config_file, "notification", "message", &message) == 0 )
    {
        g_free(event->message);
        event->message = message;
    }
    if ( libeventd_config_key_file_get_string(config_file, "notification", "icon", &icon) == 0 )
    {
        g_free(event->icon);
        event->icon = icon;
    }
    if ( libeventd_config_key_file_get_string(config_file, "notification", "overlay-icon", &overlay_icon) == 0 )
    {
        g_free(event->overlay_icon);
        event->overlay_icon = overlay_icon;
    }

    eventd_nd_style_update(event->style, config_file);
}

static EventdNdEvent *
_eventd_nd_event_new(EventdPluginContext *context, EventdNdEvent *parent)
{
    EventdNdEvent *event = NULL;

    event = g_new0(EventdNdEvent, 1);

    if ( parent == NULL )
    {
        event->title = g_strdup("$name");
        event->message = g_strdup("$text");
        event->icon = g_strdup("icon");
        event->overlay_icon = g_strdup("overlay-icon");
        event->style = eventd_nd_style_new(context->style);
    }
    else
    {
        event->title = g_strdup(parent->title);
        event->message = g_strdup(parent->message);
        event->icon = g_strdup(parent->icon);
        event->overlay_icon = g_strdup(parent->overlay_icon);
        event->style = eventd_nd_style_new(parent->style);
    }

    return event;
}

static void
_eventd_nd_event_free(gpointer data)
{
    EventdNdEvent *event = data;

    g_free(event->icon);
    g_free(event->overlay_icon);
    g_free(event->message);
    g_free(event->title);
    g_free(event);
}

static void
_eventd_nd_event_parse(EventdPluginContext *context, const gchar *event_category, const gchar *event_name, GKeyFile *config_file)
{
    gboolean disable;
    EventdNdEvent *event = NULL;

    if ( ! g_key_file_has_group(config_file, "notification") )
        return;

    if ( libeventd_config_key_file_get_boolean(config_file, "notification", "disable", &disable) < 0 )
        return;

    if ( ! disable )
    {
        event = _eventd_nd_event_new(context, ( event_name != NULL ) ? g_hash_table_lookup(context->events, event_category) : NULL);
        _eventd_nd_event_update(event, config_file);
    }

    g_hash_table_insert(context->events, libeventd_config_events_get_name(event_category, event_name), event);
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

static EventdPluginContext *
_eventd_nd_init(EventdCoreContext *core, EventdCoreInterface *interface)
{
    EventdPluginContext *context;

    context = g_new0(EventdPluginContext, 1);

    context->events = libeventd_config_events_new(_eventd_nd_event_free);
    context->surfaces = g_hash_table_new_full(g_direct_hash, g_direct_equal, g_object_unref, _eventd_nd_surface_hide_all);

    context->daemon = eventd_nd_init();

    eventd_nd_notification_init();

    /* default bubble position */
    context->bubble_anchor    = EVENTD_ND_STYLE_ANCHOR_TOP_RIGHT;
    context->bubble_margin    = 13;

    context->style = eventd_nd_style_new(NULL);

    return context;
}

static void
_eventd_nd_uninit(EventdPluginContext *context)
{
    eventd_nd_style_free(context->style);

    eventd_nd_notification_uninit();

    eventd_nd_uninit(context->daemon);

    g_hash_table_unref(context->surfaces);
    g_hash_table_unref(context->events);

    g_free(context);
}

static void
_eventd_nd_control_command(EventdPluginContext *context, const gchar *command)
{
    eventd_nd_control_command(context->daemon, command, context->bubble_anchor, context->bubble_margin);
}

static void
_eventd_nd_global_parse(EventdPluginContext *context, GKeyFile *config_file)
{
    if ( g_key_file_has_group(config_file, "nd") )
    {
        Int integer;
        gchar *string;

        if ( libeventd_config_key_file_get_string(config_file, "nd", "anchor", &string) == 0 )
        {
            if ( g_strcmp0(string, "top left") == 0 )
                context->bubble_anchor = EVENTD_ND_STYLE_ANCHOR_TOP_LEFT;
            else if ( g_strcmp0(string, "top right") == 0 )
                context->bubble_anchor = EVENTD_ND_STYLE_ANCHOR_TOP_RIGHT;
            else if ( g_strcmp0(string, "bottom left") == 0 )
                context->bubble_anchor = EVENTD_ND_STYLE_ANCHOR_BOTTOM_LEFT;
            else if ( g_strcmp0(string, "bottom right") == 0 )
                context->bubble_anchor = EVENTD_ND_STYLE_ANCHOR_BOTTOM_RIGHT;
            else
                g_warning("Wrong anchor value '%s'", string);
            g_free(string);
        }

        if ( libeventd_config_key_file_get_int(config_file, "nd", "margin", &integer) == 0 )
            context->bubble_margin = integer.value;
    }

    eventd_nd_style_update(context->style, config_file);
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
    EventdNdNotification *notification;
    GList *surface;

    nd_event = libeventd_config_events_get_event(context->events, eventd_event_get_category(event), eventd_event_get_name(event));
    if ( nd_event == NULL )
        return;

    notification = eventd_nd_notification_new(event, nd_event->title, nd_event->message, nd_event->icon, nd_event->overlay_icon);


    surface = eventd_nd_event_action(context->daemon, event, notification, nd_event->style);
    if ( surface != NULL )
    {
        g_signal_connect(event, "ended", G_CALLBACK(_eventd_nd_event_ended), context);

        /*
         * TODO: Update an existing bubble
         */

        g_hash_table_insert(context->surfaces, g_object_ref(event), surface);
    }

    eventd_nd_notification_free(notification);
}

static void
_eventd_nd_config_reset(EventdPluginContext *context)
{
    g_hash_table_remove_all(context->events);
}

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

