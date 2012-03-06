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
#include <libeventd-regex.h>

#include "nd.h"
#include "daemon/daemon.h"


struct _EventdPluginContext {
    GHashTable *events;
    EventdNdContext *daemon;
};

static void
_eventd_nd_event_update(EventdNdEvent *event, gboolean disable, const char *title, const char *message, const char *icon, const char *overlay_icon, Int *scale)
{
    event->disable = disable;
    if ( title != NULL )
    {
        g_free(event->title);
        event->title = g_strdup(title);
    }
    if ( message != NULL )
    {
        g_free(event->message);
        event->message = g_strdup(message);
    }
    if ( icon != NULL )
    {
        g_free(event->icon);
        event->icon = g_strdup(icon);
    }
    if ( overlay_icon != NULL )
    {
        g_free(event->overlay_icon);
        event->overlay_icon = g_strdup(overlay_icon);
    }
    if ( scale->set )
        event->scale = (gdouble)scale->value / 100.;
}

static EventdNdEvent *
_eventd_nd_event_new(gboolean disable, const char *title, const char *message, const char *icon, const char *overlay_icon, Int *scale, EventdNdEvent *parent)
{
    EventdNdEvent *event = NULL;

    title = ( title != NULL ) ? title : ( parent != NULL ) ? parent->title : "$client-name - $name";
    message = ( message != NULL ) ? message : ( parent != NULL ) ? parent->message : "$text";
    icon = ( icon != NULL ) ? icon : ( parent != NULL ) ? parent->icon : "icon";
    overlay_icon = ( overlay_icon != NULL ) ? overlay_icon : ( parent != NULL ) ? parent->overlay_icon : "overlay-icon";
    scale->value = scale->set ? scale->value : ( parent != NULL ) ? parent->scale * 100 : 50;
    scale->set = TRUE;

    event = g_new0(EventdNdEvent, 1);

    _eventd_nd_event_update(event, disable, title, message, icon, overlay_icon, scale);

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
    gchar *name = NULL;
    gchar *title = NULL;
    gchar *message = NULL;
    gchar *icon = NULL;
    gchar *overlay_icon = NULL;
    Int scale;
    EventdNdEvent *event;

    if ( ! g_key_file_has_group(config_file, "notification") )
        return;

    if ( libeventd_config_key_file_get_boolean(config_file, "notification", "disable", &disable) < 0 )
        goto skip;
    if ( libeventd_config_key_file_get_string(config_file, "notification", "title", &title) < 0 )
        goto skip;
    if ( libeventd_config_key_file_get_string(config_file, "notification", "message", &message) < 0 )
        goto skip;
    if ( libeventd_config_key_file_get_string(config_file, "notification", "icon", &icon) < 0 )
        goto skip;
    if ( libeventd_config_key_file_get_string(config_file, "notification", "overlay-icon", &overlay_icon) < 0 )
        goto skip;
    if ( libeventd_config_key_file_get_int(config_file, "notification", "overlay-scale", &scale) < 0 )
        goto skip;

    name = libeventd_config_events_get_name(event_category, event_name);

    event = g_hash_table_lookup(context->events, name);
    if ( event != NULL )
        _eventd_nd_event_update(event, disable, title, message, icon, overlay_icon, &scale);
    else
        g_hash_table_insert(context->events, name, _eventd_nd_event_new(disable, title, message, icon, overlay_icon, &scale, g_hash_table_lookup(context->events, event_category)));

skip:
    g_free(overlay_icon);
    g_free(icon);
    g_free(message);
    g_free(title);
}

static void
_eventd_nd_notification_icon_data_from_file(gchar *path, guchar **data, gsize *length)
{
    GError *error = NULL;

    if ( *path == 0 )
        return;

    if ( ! g_file_get_contents(path, (gchar **)data, length, &error) )
        g_warning("Couldn’t load file '%s': %s", path, error->message);
    g_clear_error(&error);

    g_free(path);
}

static void
_eventd_nd_notification_icon_data_from_base64(EventdEvent *event, const gchar *name, guchar **data, gsize *length, const gchar **format)
{
    const gchar *base64;
    gchar *format_name;

    base64 = eventd_event_get_data(event, name);
    if ( base64 != NULL )
        *data = g_base64_decode(base64, length);

    format_name = g_strconcat(name, "-format", NULL);
    *format = eventd_event_get_data(event, format_name);
    g_free(format_name);
}

static EventdNdNotification *
_eventd_nd_notification_new(EventdEvent *event, EventdNdEvent *nd_event)
{
    EventdNdNotification *notification;
    gchar *icon;

    notification = g_new0(EventdNdNotification, 1);

    notification->title = libeventd_regex_replace_event_data(nd_event->title, event, NULL, NULL);

    notification->message = libeventd_regex_replace_event_data(nd_event->message, event, NULL, NULL);

    if ( ( icon = libeventd_config_get_filename(nd_event->icon, event, "icons") ) != NULL )
        _eventd_nd_notification_icon_data_from_file(icon, &notification->icon, &notification->icon_length);
    else
        _eventd_nd_notification_icon_data_from_base64(event, nd_event->icon, &notification->icon, &notification->icon_length, &notification->icon_format);

    if ( ( icon = libeventd_config_get_filename(nd_event->overlay_icon, event, "icons") ) != NULL )
        _eventd_nd_notification_icon_data_from_file(icon, &notification->overlay_icon, &notification->overlay_icon_length);
    else
        _eventd_nd_notification_icon_data_from_base64(event, nd_event->overlay_icon, &notification->overlay_icon, &notification->overlay_icon_length, &notification->overlay_icon_format);

    return notification;
}


static void
_eventd_nd_notification_free(EventdNdNotification *notification)
{
    g_free(notification->overlay_icon);
    g_free(notification->icon);
    g_free(notification->message);
    g_free(notification->title);

    g_free(notification);
}

static EventdPluginContext *
_eventd_nd_init(EventdCoreContext *core, EventdCoreInterface *interface)
{
    EventdPluginContext *context;

    context = g_new0(EventdPluginContext, 1);

    context->events = libeventd_config_events_new(_eventd_nd_event_free);

    context->daemon = eventd_nd_init();

    libeventd_regex_init();

    return context;
}

static void
_eventd_nd_uninit(EventdPluginContext *context)
{
    libeventd_regex_clean();

    eventd_nd_uninit(context->daemon);

    g_hash_table_unref(context->events);

    g_free(context);
}

static void
_eventd_nd_control_command(EventdPluginContext *context, const gchar *command)
{
    eventd_nd_control_command(context->daemon, command);
}

static void
_eventd_nd_event_action(EventdPluginContext *context, EventdEvent *event)
{
    EventdNdEvent *nd_event;
    EventdNdNotification *notification;

    nd_event = libeventd_config_events_get_event(context->events, eventd_event_get_category(event), eventd_event_get_name(event));
    if ( nd_event == NULL )
        return;

    if ( nd_event->disable )
        return;

    notification = _eventd_nd_notification_new(event, nd_event);

    eventd_nd_event_action(context->daemon, event, notification);

    _eventd_nd_notification_free(notification);
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

    plugin->event_parse = _eventd_nd_event_parse;
    plugin->event_action = _eventd_nd_event_action;
}

