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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <glib.h>
#include <glib-object.h>

#include <libnotify/notify.h>
#include "libnotify-compat.h"
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libeventd-event.h>
#include <eventd-plugin.h>
#include <libeventd-regex.h>
#include <libeventd-config.h>

#include "image.h"

struct _EventdPluginContext {
    GHashTable *events;
};

typedef struct {
    gboolean disable;
    gchar *title;
    gchar *message;
    gchar *image;
    gchar *icon;
    gdouble scale;
} EventdLibnotifyEvent;


/*
 * Event contents helper
 */

static EventdLibnotifyEvent *
_eventd_libnotify_event_new(gboolean disable, const char *title, const char *message, const char *image, const char *icon, Int *scale)
{
    EventdLibnotifyEvent *event;

    event = g_new0(EventdLibnotifyEvent, 1);

    event->title = g_strdup(( title != NULL ) ? title : "$summary");
    event->message = g_strdup(( message != NULL ) ? message : "$body");
    event->image = g_strdup(( image != NULL ) ? image : "image");
    event->icon = g_strdup(( icon != NULL ) ? icon : "icon");
    event->scale = scale->set ? ( scale->value / 100. ) : 0.5;

    return event;
}

static void
_eventd_libnotify_event_free(gpointer data)
{
    EventdLibnotifyEvent *event = data;

    g_free(event->image);
    g_free(event->icon);
    g_free(event->message);
    g_free(event->title);
    g_free(event);
}


/*
 * Initialization interface
 */

static EventdPluginContext *
_eventd_libnotify_init()
{
    EventdPluginContext *context;

    notify_init(PACKAGE_NAME);

    if ( ! notify_is_initted() )
    {
        g_warning("Couldn't initialize notify system");
        return NULL;
    }

    libeventd_regex_init();

    context = g_new0(EventdPluginContext, 1);

    context->events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, _eventd_libnotify_event_free);

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


/*
 * Configuration interface
 */

static void
_eventd_libnotify_event_parse(EventdPluginContext *context, const gchar *id, GKeyFile *config_file)
{
    gboolean disable;
    gchar *title = NULL;
    gchar *message = NULL;
    gchar *image = NULL;
    gchar *icon = NULL;
    Int scale;

    if ( ! g_key_file_has_group(config_file, "Libnotify") )
        return;

    if ( libeventd_config_key_file_get_boolean(config_file, "Libnotify", "Disable", &disable) < 0 )
        goto skip;
    if ( libeventd_config_key_file_get_string(config_file, "Libnotify", "Title", &title) < 0 )
        goto skip;
    if ( libeventd_config_key_file_get_string(config_file, "Libnotify", "Message", &message) < 0 )
        goto skip;
    if ( libeventd_config_key_file_get_string(config_file, "Libnotify", "Image", &image) < 0 )
        goto skip;
    if ( libeventd_config_key_file_get_string(config_file, "Libnotify", "Icon", &icon) < 0 )
        goto skip;
    if ( libeventd_config_key_file_get_int(config_file, "Libnotify", "OverlayScale", &scale) < 0 )
        goto skip;

    g_hash_table_insert(context->events, g_strdup(id), _eventd_libnotify_event_new(disable, title, message, image, icon, &scale));

skip:
    g_free(icon);
    g_free(image);
    g_free(message);
    g_free(title);
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

    title = libeventd_regex_replace_event_data(libnotify_event->title, event, NULL, NULL);

    message = libeventd_regex_replace_event_data(libnotify_event->message, event, NULL, NULL);

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
    interface->init   = _eventd_libnotify_init;
    interface->uninit = _eventd_libnotify_uninit;

    interface->event_parse  = _eventd_libnotify_event_parse;
    interface->config_reset = _eventd_libnotify_config_reset;

    interface->event_action = _eventd_libnotify_event_action;
}

