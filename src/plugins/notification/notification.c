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

#include "notification.h"
#include "daemon/daemon.h"
#include "notify.h"


struct _EventdPluginContext {
    GHashTable *events;
    EventdNdContext *daemon;
};

static EventdPluginContext *
_eventd_notification_start(gpointer user_data)
{
    EventdPluginContext *context;

    context = g_new0(EventdPluginContext, 1);

    context->daemon = eventd_nd_init();
    eventd_notification_notify_init();

    libeventd_regex_init();

    return context;
}

static void
_eventd_notification_stop(EventdPluginContext *context)
{
    libeventd_regex_clean();

    eventd_notification_notify_uninit();
    eventd_nd_uninit(context->daemon);

    g_free(context);
}

static void
_eventd_notification_control_command(EventdPluginContext *context, const gchar *command)
{
    eventd_nd_control_command(context->daemon, command);
}

static void
_eventd_notification_event_update(EventdNotificationEvent *event, gboolean disable, const char *title, const char *message, const char *icon, const char *overlay_icon, Int *scale)
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

static EventdNotificationEvent *
_eventd_notification_event_new(gboolean disable, const char *title, const char *message, const char *icon, const char *overlay_icon, Int *scale, EventdNotificationEvent *parent)
{
    EventdNotificationEvent *event = NULL;

    title = ( title != NULL ) ? title : ( parent != NULL ) ? parent->title : "$client-name - $name";
    message = ( message != NULL ) ? message : ( parent != NULL ) ? parent->message : "$text";
    icon = ( icon != NULL ) ? icon : ( parent != NULL ) ? parent->icon : "icon";
    overlay_icon = ( overlay_icon != NULL ) ? overlay_icon : ( parent != NULL ) ? parent->overlay_icon : "overlay-icon";
    scale->value = scale->set ? scale->value : ( parent != NULL ) ? parent->scale * 100 : 50;
    scale->set = TRUE;

    event = g_new0(EventdNotificationEvent, 1);

    _eventd_notification_event_update(event, disable, title, message, icon, overlay_icon, scale);

    return event;
}

static void
_eventd_notification_event_free(gpointer data)
{
    EventdNotificationEvent *event = data;

    g_free(event->icon);
    g_free(event->overlay_icon);
    g_free(event->message);
    g_free(event->title);
    g_free(event);
}

static void
_eventd_notification_event_parse(EventdPluginContext *context, const gchar *event_category, const gchar *event_name, GKeyFile *config_file)
{
    gboolean disable;
    gchar *name = NULL;
    gchar *title = NULL;
    gchar *message = NULL;
    gchar *icon = NULL;
    gchar *overlay_icon = NULL;
    Int scale;
    EventdNotificationEvent *event;

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
        _eventd_notification_event_update(event, disable, title, message, icon, overlay_icon, &scale);
    else
        g_hash_table_insert(context->events, name, _eventd_notification_event_new(disable, title, message, icon, overlay_icon, &scale, g_hash_table_lookup(context->events, event_category)));

skip:
    g_free(overlay_icon);
    g_free(icon);
    g_free(message);
    g_free(title);
}

static void
_eventd_notification_notification_icon_data_from_file(gchar *path, guchar **data, gsize *length)
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
_eventd_notification_notification_icon_data_from_base64(GHashTable *event_data, const gchar *name, guchar **data, gsize *length, gchar **format)
{
    const gchar *base64;
    gchar *format_name;

    base64 = g_hash_table_lookup(event_data, name);
    if ( base64 != NULL )
        *data = g_base64_decode(base64, length);

    format_name = g_strconcat(name, "-format", NULL);
    *format = g_hash_table_lookup(event_data, format_name);
    g_free(format_name);
}

static EventdNotificationNotification *
_eventd_notification_notification_new(GHashTable *data, EventdNotificationEvent *notification_event)
{
    EventdNotificationNotification *notification;
    gchar *icon;

    notification = g_new0(EventdNotificationNotification, 1);

    notification->title = libeventd_regex_replace_event_data(notification_event->title, data, NULL);

    notification->message = libeventd_regex_replace_event_data(notification_event->message, data, NULL);

    if ( ( icon = libeventd_config_get_filename(notification_event->icon, data, "icons") ) != NULL )
        _eventd_notification_notification_icon_data_from_file(icon, &notification->icon, &notification->icon_length);
    else
        _eventd_notification_notification_icon_data_from_base64(data, notification_event->icon, &notification->icon, &notification->icon_length, &notification->icon_format);

    if ( ( icon = libeventd_config_get_filename(notification_event->overlay_icon, data, "icons") ) != NULL )
        _eventd_notification_notification_icon_data_from_file(icon, &notification->overlay_icon, &notification->overlay_icon_length);
    else
        _eventd_notification_notification_icon_data_from_base64(data, notification_event->overlay_icon, &notification->overlay_icon, &notification->overlay_icon_length, &notification->overlay_icon_format);

    return notification;
}


static void
_eventd_notification_notification_add_pong_data(EventdEvent *event, EventdNotificationNotification *notification)
{
    eventd_event_add_pong_data(event, g_strdup("notification-title"), g_strdup(notification->title));
    eventd_event_add_pong_data(event, g_strdup("notification-message"), g_strdup(notification->message));

    if ( notification->icon != NULL )
        eventd_event_add_pong_data(event, g_strdup("notification-icon"), g_base64_encode(notification->icon, notification->icon_length));
    if ( notification->overlay_icon != NULL )
        eventd_event_add_pong_data(event, g_strdup("notification-overlay-icon"), g_base64_encode(notification->overlay_icon, notification->overlay_icon_length));
}

static void
_eventd_notification_notification_free(EventdNotificationNotification *notification)
{
    g_free(notification->overlay_icon);
    g_free(notification->icon);
    g_free(notification->message);
    g_free(notification->title);

    g_free(notification);
}

static void
_eventd_notification_event_action(EventdPluginContext *context, EventdEvent *event)
{
    EventdNotificationEvent *notification_event;
    EventdNotificationNotification *notification;

    notification_event = libeventd_config_events_get_event(context->events, eventd_event_get_category(event), eventd_event_get_name(event));
    if ( notification_event == NULL )
        return;

    if ( notification_event->disable )
        return;

    notification = _eventd_notification_notification_new(eventd_event_get_data(event), notification_event);

    eventd_nd_event_action(context->daemon, event, notification);
    eventd_notification_notify_event_action(notification, eventd_event_get_timeout(event), notification_event->scale);

    _eventd_notification_notification_free(notification);
}

static void
_eventd_notification_event_pong(EventdPluginContext *context, EventdEvent *event)
{
    EventdNotificationEvent *notification_event;
    EventdNotificationNotification *notification;

    notification_event = libeventd_config_events_get_event(context->events, eventd_event_get_category(event), eventd_event_get_name(event));
    if ( notification_event == NULL )
        return;

    if ( notification_event->disable )
        return;

    notification = _eventd_notification_notification_new(eventd_event_get_data(event), notification_event);

    _eventd_notification_notification_add_pong_data(event, notification);

    _eventd_notification_notification_free(notification);
}


static void
_eventd_notification_config_init(EventdPluginContext *context)
{
    context->events = libeventd_config_events_new(_eventd_notification_event_free);
}

static void
_eventd_notification_config_clean(EventdPluginContext *context)
{
    g_hash_table_unref(context->events);
}

void
eventd_plugin_get_info(EventdPlugin *plugin)
{
    plugin->start = _eventd_notification_start;
    plugin->stop = _eventd_notification_stop;

    plugin->control_command = _eventd_notification_control_command;

    plugin->config_init = _eventd_notification_config_init;
    plugin->config_clean = _eventd_notification_config_clean;

    plugin->event_parse = _eventd_notification_event_parse;
    plugin->event_action = _eventd_notification_event_action;
    plugin->event_pong = _eventd_notification_event_pong;
}

