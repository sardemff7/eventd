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

#include <config.h>

#include <glib.h>
#include <glib-object.h>

#include <libnotify/notify.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libeventd-event.h>
#include <eventd-plugin.h>
#include <libeventd-config.h>

#include "image.h"

struct _EventdPluginContext {
    GHashTable *events;
};

typedef struct {
    FormatString *title;
    FormatString *message;
    Filename *image;
    Filename *icon;
    gdouble scale;
    NotifyUrgency urgency;
} EventdLibnotifyEvent;


/*
 * Event contents helper
 */

static EventdLibnotifyEvent *
_eventd_libnotify_event_new(FormatString *title, FormatString *message, Filename *image, Filename *icon, gint64 scale, gchar *urgency)
{
    EventdLibnotifyEvent *event;

    event = g_new0(EventdLibnotifyEvent, 1);

    event->title = title;
    event->message = message;
    event->image = image;
    event->icon = icon;
    event->scale = (gdouble) scale / 100.;
    event->urgency = NOTIFY_URGENCY_NORMAL;
    if ( g_strcmp0(urgency, "low") == 0 )
        event->urgency = NOTIFY_URGENCY_LOW;
    else if ( g_strcmp0(urgency, "normal") == 0 )
        event->urgency = NOTIFY_URGENCY_NORMAL;
    else if ( g_strcmp0(urgency, "critical") == 0 )
        event->urgency = NOTIFY_URGENCY_CRITICAL;
    else
        g_warning("Unknown urgency: %s", urgency);
    g_free(urgency);

    return event;
}

static void
_eventd_libnotify_event_free(gpointer data)
{
    EventdLibnotifyEvent *event = data;

    libeventd_filename_unref(event->image);
    libeventd_filename_unref(event->icon);
    libeventd_format_string_unref(event->message);
    libeventd_format_string_unref(event->title);

    g_free(event);
}


/*
 * Initialization interface
 */

static EventdPluginContext *
_eventd_libnotify_init(EventdPluginCoreContext *core, EventdPluginCoreInterface *interface)
{
    EventdPluginContext *context;

    notify_init(PACKAGE_NAME);

    if ( ! notify_is_initted() )
    {
        g_warning("Couldn't initialize notify system");
        return NULL;
    }

    gchar *server_name;
    notify_get_server_info(&server_name, NULL, NULL, NULL);

    if ( g_strcmp0(server_name, PACKAGE_NAME) == 0 )
    {
        g_debug("We would send notifications to ourselves: quitting");

        g_free(server_name);
        notify_uninit();
        return NULL;
    }
    g_free(server_name);

    context = g_new0(EventdPluginContext, 1);

    context->events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, _eventd_libnotify_event_free);

    return context;
}

static void
_eventd_libnotify_uninit(EventdPluginContext *context)
{
    g_hash_table_unref(context->events);

    g_free(context);

    notify_uninit();
}


/*
 * Configuration interface
 */

static void
_eventd_libnotify_event_parse(EventdPluginContext *context, const gchar *id, GKeyFile *config_file)
{
    gboolean disable;
    EventdLibnotifyEvent *libnotify_event = NULL;
    FormatString *title = NULL;
    FormatString *message = NULL;
    Filename *image = NULL;
    Filename *icon = NULL;
    gint64 scale;
    gchar *urgency = NULL;

    if ( ! g_key_file_has_group(config_file, "Libnotify") )
        return;

    if ( libeventd_config_key_file_get_boolean(config_file, "Libnotify", "Disable", &disable) < 0 )
        return;

    if ( ! disable )
    {
        if ( libeventd_config_key_file_get_locale_format_string_with_default(config_file, "Libnotify", "Title", NULL, "${summary}", &title) < 0 )
            goto skip;
        if ( libeventd_config_key_file_get_locale_format_string_with_default(config_file, "Libnotify", "Message", NULL, "${body}", &message) < 0 )
            goto skip;
        if ( libeventd_config_key_file_get_filename_with_default(config_file, "Libnotify", "Image", "image", &image) < 0 )
            goto skip;
        if ( libeventd_config_key_file_get_filename_with_default(config_file, "Libnotify", "Icon", "icon", &icon) < 0 )
            goto skip;
        if ( libeventd_config_key_file_get_int_with_default(config_file, "Libnotify", "OverlayScale", 50, &scale) < 0 )
            goto skip;
        if ( libeventd_config_key_file_get_string_with_default(config_file, "Libnotify", "Urgency", "normal", &urgency) < 0 )
            goto skip;

        libnotify_event = _eventd_libnotify_event_new(title, message, image, icon, scale, urgency);
        title = message = NULL;
        image = icon = NULL;
        urgency = NULL;
    }

    g_hash_table_insert(context->events, g_strdup(id), libnotify_event);

skip:
    g_free(urgency);
    libeventd_filename_unref(icon);
    libeventd_filename_unref(image);
    libeventd_format_string_unref(message);
    libeventd_format_string_unref(title);
}

static void
_eventd_libnotify_config_reset(EventdPluginContext *context)
{
    g_hash_table_remove_all(context->events);
}


/*
 * Event action interface
 */

static void
_eventd_libnotify_event_action(EventdPluginContext *context, const gchar *config_id, EventdEvent *event)
{
    GError *error = NULL;
    EventdLibnotifyEvent *libnotify_event;
    gchar *title;
    gchar *message;
    gchar *icon_uri = NULL;
    NotifyNotification *notification = NULL;
    GdkPixbuf *image;

    libnotify_event = g_hash_table_lookup(context->events, config_id);
    if ( libnotify_event == NULL )
        return;

    title = libeventd_format_string_get_string(libnotify_event->title, event, NULL, NULL);
    message = libeventd_format_string_get_string(libnotify_event->message, event, NULL, NULL);

    image = eventd_libnotify_get_image(event, libnotify_event->image, libnotify_event->icon, libnotify_event->scale, &icon_uri);

    notification = notify_notification_new(title, message, icon_uri);
    g_free(icon_uri);
    g_free(message);
    g_free(title);

    if ( image != NULL )
    {
        notify_notification_set_image_from_pixbuf(notification, image);
        g_object_unref(image);
    }

    notify_notification_set_urgency(notification, libnotify_event->urgency);
    notify_notification_set_timeout(notification, eventd_event_get_timeout(event));

    if ( ! notify_notification_show(notification, &error) )
        g_warning("Can't show the notification: %s", error->message);
    g_clear_error(&error);

    g_object_unref(notification);
}


/*
 * Plugin interface
 */

EVENTD_EXPORT const gchar *eventd_plugin_id = "eventd-notify";
EVENTD_EXPORT
void
eventd_plugin_get_interface(EventdPluginInterface *interface)
{
    eventd_plugin_interface_add_init_callback(interface, _eventd_libnotify_init);
    eventd_plugin_interface_add_uninit_callback(interface, _eventd_libnotify_uninit);

    eventd_plugin_interface_add_event_parse_callback(interface, _eventd_libnotify_event_parse);
    eventd_plugin_interface_add_config_reset_callback(interface, _eventd_libnotify_config_reset);

    eventd_plugin_interface_add_event_action_callback(interface, _eventd_libnotify_event_action);
}

