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

#include <libnotify/notify.h>
#include "libnotify-compat.h"
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <glib.h>
#include <glib-object.h>

#include <libeventd-event.h>
#include <eventd-plugin.h>
#include <libeventd-regex.h>
#include <libeventd-config.h>

#include "icon.h"
#include "event.h"

struct _EventdPluginContext {
    GHashTable *events;
};

static EventdPluginContext *
_eventd_libnotify_init()
{
    EventdPluginContext *context;

    notify_init(PACKAGE_NAME);

    if ( ! notify_is_initted() )
    {
        g_warning("Couldn’t initialize notify system");
        return NULL;
    }

    libeventd_regex_init();

    context = g_new0(EventdPluginContext, 1);

    context->events = libeventd_config_events_new(eventd_libnotify_event_free);

    return context;
}

static void
_eventd_libnotify_uninit(EventdPluginContext *context)
{
    g_hash_table_unref(context->events);

    g_free(context);

    libeventd_regex_clean();

    notify_uninit();
}

static void
eventd_libnotify_event_parse(EventdPluginContext *context, const gchar *event_category, const gchar *event_name, GKeyFile *config_file)
{
    gboolean disable;
    gchar *name = NULL;
    gchar *title = NULL;
    gchar *message = NULL;
    gchar *icon = NULL;
    gchar *overlay_icon = NULL;
    Int scale;
    EventdLibnotifyEvent *event;

    if ( ! g_key_file_has_group(config_file, "Libnotify") )
        return;

    if ( libeventd_config_key_file_get_boolean(config_file, "Libnotify", "Disable", &disable) < 0 )
        goto skip;
    if ( libeventd_config_key_file_get_string(config_file, "Libnotify", "Title", &title) < 0 )
        goto skip;
    if ( libeventd_config_key_file_get_string(config_file, "Libnotify", "Message", &message) < 0 )
        goto skip;
    if ( libeventd_config_key_file_get_string(config_file, "Libnotify", "Icon", &icon) < 0 )
        goto skip;
    if ( libeventd_config_key_file_get_string(config_file, "Libnotify", "OverlayIcon", &overlay_icon) < 0 )
        goto skip;
    if ( libeventd_config_key_file_get_int(config_file, "Libnotify", "OverlayScale", &scale) < 0 )
        goto skip;

    name = libeventd_config_events_get_name(event_category, event_name);

    event = g_hash_table_lookup(context->events, name);
    if ( event != NULL )
        eventd_libnotify_event_update(event, disable, title, message, icon, overlay_icon, &scale);
    else
        g_hash_table_insert(context->events, name, eventd_libnotify_event_new(disable, title, message, icon, overlay_icon, &scale, g_hash_table_lookup(context->events, event_category)));

skip:
    g_free(overlay_icon);
    g_free(icon);
    g_free(message);
    g_free(title);
}

static void
eventd_libnotify_event_action(EventdPluginContext *context, EventdEvent *event)
{
    GError *error = NULL;
    EventdLibnotifyEvent *libnotify_event;
    gchar *title;
    gchar *message;
    gchar *icon_uri = NULL;
    NotifyNotification *notification = NULL;
    GdkPixbuf *icon;

    libnotify_event = libeventd_config_events_get_event(context->events, eventd_event_get_category(event), eventd_event_get_name(event));
    if ( libnotify_event == NULL )
        return;

    title = libeventd_regex_replace_event_data(libnotify_event->title, event, NULL, NULL);

    message = libeventd_regex_replace_event_data(libnotify_event->message, event, NULL, NULL);

    icon = eventd_libnotify_get_icon(event, libnotify_event->icon, libnotify_event->overlay_icon, libnotify_event->scale, &icon_uri);

    notification = notify_notification_new(title, message, icon_uri);
    g_free(icon_uri);
    g_free(message);
    g_free(title);

    if ( icon != NULL )
    {
        notify_notification_set_image_from_pixbuf(notification, icon);
        g_object_unref(icon);
    }


    notify_notification_set_timeout(notification, eventd_event_get_timeout(event));

    if ( ! notify_notification_show(notification, &error) )
        g_warning("Can't show the notification: %s", error->message);
    g_clear_error(&error);

    g_object_unref(notification);
}

static void
_eventd_libnotify_config_reset(EventdPluginContext *context)
{
    g_hash_table_remove_all(context->events);
}

const gchar *eventd_plugin_id = "eventd-notify";
void
eventd_plugin_get_info(EventdPlugin *plugin)
{
    plugin->init = _eventd_libnotify_init;
    plugin->uninit = _eventd_libnotify_uninit;

    plugin->config_reset = _eventd_libnotify_config_reset;

    plugin->event_parse = eventd_libnotify_event_parse;
    plugin->event_action = eventd_libnotify_event_action;
}

