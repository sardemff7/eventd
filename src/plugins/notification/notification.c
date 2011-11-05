/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011 Quentin "Sardem FF7" Glidic
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
#if ! NOTIFY_CHECK_VERSION(0,7,0)
#include "libnotify-compat.h"
#endif

#include <glib.h>

#include <eventd-plugin.h>
#include <plugin-helper.h>


typedef struct {
    gboolean disable;
    gchar *title;
    gchar *message;
    gchar *icon;
    gchar *overlay_icon;
    gdouble scale;
    gint64 timeout;
} EventdNotificationEvent;

static GHashTable *events = NULL;

static void
eventd_notification_start(gpointer user_data)
{
    notify_init(PACKAGE_NAME);

    eventd_plugin_helper_regex_init();
}

static void
eventd_notification_stop()
{
    eventd_plugin_helper_regex_clean();

    notify_uninit();
}

static gboolean
notification_closed_cb(NotifyNotification *notification)
{
    return FALSE;
}

static void
eventd_notification_event_clean(EventdNotificationEvent *event)
{
    g_free(event->message);
    g_free(event->title);
}

static void
eventd_notification_event_update(EventdNotificationEvent *event, gboolean disable, const char *title, const char *message, const char *icon, const char *overlay_icon, Int *scale, Int *timeout)
{
    eventd_notification_event_clean(event);
    event->disable = disable;
    if ( title != NULL )
        event->title = g_strdup(title);
    if ( message != NULL )
        event->message = g_strdup(message);
    if ( icon != NULL )
        event->icon = g_strdup(icon);
    if ( overlay_icon != NULL )
        event->overlay_icon = g_strdup(overlay_icon);
    if ( scale->set )
        event->scale = (gdouble)scale->value / 100.;
    if ( timeout->set )
        event->timeout = ( timeout->value > 0 ) ? ( timeout->value * 1000 ) : timeout->value;
}

static EventdNotificationEvent *
eventd_notification_event_new(gboolean disable, const char *title, const char *message, const char *icon, const char *overlay_icon, Int *scale, Int *timeout, EventdNotificationEvent *parent)
{
    EventdNotificationEvent *event = NULL;

    title = title ?: parent ? parent->title : "$client-name - $event-data[name]";
    message = message ?: parent ? parent->message : "$event-data[text]";
    icon = icon ?: parent ? parent->icon : "icon";
    overlay_icon = overlay_icon ?: parent ? parent->overlay_icon : "overlay-icon";
    scale->value = scale->set ? scale->value : parent ? parent->scale : 50;
    scale->set = TRUE;
    timeout->value = timeout->set ? timeout->value : parent ? parent->timeout : NOTIFY_EXPIRES_DEFAULT;
    timeout->set = TRUE;

    event = g_new0(EventdNotificationEvent, 1);

    eventd_notification_event_update(event, disable, title, message, icon, overlay_icon, scale, timeout);

    return event;
}

static void
eventd_notification_event_free(EventdNotificationEvent *event)
{
    eventd_notification_event_clean(event);
    g_free(event);
}

static void
eventd_notification_event_parse(const gchar *client_type, const gchar *event_type, GKeyFile *config_file)
{
    gboolean disable;
    gchar *name = NULL;
    gchar *title = NULL;
    gchar *message = NULL;
    gchar *icon = NULL;
    gchar *overlay_icon = NULL;
    Int scale;
    Int timeout;
    EventdNotificationEvent *event;

    if ( ! g_key_file_has_group(config_file, "notification") )
        return;

    if ( eventd_plugin_helper_config_key_file_get_boolean(config_file, "notification", "disable", &disable) < 0 )
        goto skip;
    if ( eventd_plugin_helper_config_key_file_get_string(config_file, "notification", "title", &title) < 0 )
        goto skip;
    if ( eventd_plugin_helper_config_key_file_get_string(config_file, "notification", "message", &message) < 0 )
        goto skip;
    if ( eventd_plugin_helper_config_key_file_get_string(config_file, "notification", "icon", &icon) < 0 )
        goto skip;
    if ( eventd_plugin_helper_config_key_file_get_string(config_file, "notification", "overlay-icon", &overlay_icon) < 0 )
        goto skip;
    if ( eventd_plugin_helper_config_key_file_get_int(config_file, "notification", "overlay-scale", &scale) < 0 )
        goto skip;
    if ( eventd_plugin_helper_config_key_file_get_int(config_file, "notification", "timeout", &timeout) < 0 )
        goto skip;

    if ( event_type != NULL )
        name = g_strconcat(client_type, "-", event_type, NULL);
    else
        name = g_strdup(client_type);

    event = g_hash_table_lookup(events, name);
    if ( event != NULL )
        eventd_notification_event_update(event, disable, title, message, icon, overlay_icon, &scale, &timeout);
    else
        g_hash_table_insert(events, name, eventd_notification_event_new(disable, title, message, icon, overlay_icon, &scale, &timeout, g_hash_table_lookup(events, client_type)));

skip:
    g_free(overlay_icon);
    g_free(icon);
    g_free(message);
    g_free(title);
}

static GdkPixbuf *
eventd_notification_get_icon_pixbuf(const char *icon_name, GHashTable *event_data)
{
    GError *error = NULL;
    gchar *icon_base64 = NULL;
    guchar *icon = NULL;
    gsize icon_length;
    gchar *icon_size_name;
    gchar *icon_size_text;
    GdkPixbufLoader *loader;
    GdkPixbuf *pixbuf = NULL;

    if ( event_data == NULL )
        return NULL;

    icon_base64 = g_hash_table_lookup(event_data, icon_name);
    if ( icon_base64 == NULL )
        return NULL;

    icon_size_name = g_strconcat(icon_name, "-size", NULL);
    icon_size_text = g_hash_table_lookup(event_data, icon_size_name);
    g_free(icon_size_name);

    icon = g_base64_decode(icon_base64, &icon_length);

    loader = gdk_pixbuf_loader_new();

    if ( icon_size_text != NULL )
    {
        guint64 icon_size;
        icon_size = g_ascii_strtoull(icon_size_text, NULL, 10);
        gdk_pixbuf_loader_set_size(loader, icon_size, icon_size);
    }

    if ( ! gdk_pixbuf_loader_write(loader, icon, icon_length, &error) )
    {
        g_warning("Couldn’t write icon data: %s", error->message);
        g_clear_error(&error);
        goto fail;
    }

    if ( ! gdk_pixbuf_loader_close(loader, &error) )
    {
        g_warning("Couldn’t terminate icon data loading: %s", error->message);
        g_clear_error(&error);
        goto fail;
    }

    pixbuf = g_object_ref(gdk_pixbuf_loader_get_pixbuf(loader));

fail:
    g_free(icon);
    g_object_unref(loader);
    return pixbuf;
}

static GHashTable *
eventd_notification_event_action(EventdEvent *event)
{
    gchar *name;
    GError *error = NULL;
    EventdNotificationEvent *notification_event;
    gchar *title = NULL;
    gchar *message = NULL;
    GdkPixbuf *icon = NULL;
    GdkPixbuf *overlay_icon = NULL;
    gchar *tmp = NULL;
    NotifyNotification *notification = NULL;

    name = g_strconcat(event->client->type, "-", event->type, NULL);
    notification_event = g_hash_table_lookup(events, name);
    g_free(name);
    if ( ( notification_event == NULL ) && ( ( notification_event = g_hash_table_lookup(events, event->client->type) ) == NULL ) )
        return NULL;

    if ( notification_event->disable )
        return NULL;

    tmp = eventd_plugin_helper_regex_replace_client_name(notification_event->title, event->client->name);
    title = eventd_plugin_helper_regex_replace_event_data(tmp, event->data, NULL);
    g_free(tmp);

    message = eventd_plugin_helper_regex_replace_event_data(notification_event->message, event->data, NULL);

    notification = notify_notification_new(title, message, NULL);

    icon = eventd_notification_get_icon_pixbuf(notification_event->icon, event->data);
    overlay_icon = eventd_notification_get_icon_pixbuf(notification_event->overlay_icon, event->data);
    if ( icon != NULL )
    {
        if ( overlay_icon != NULL )
        {
            gint icon_width, icon_height;
            gint overlay_icon_width, overlay_icon_height;
            gint x, y;
            gdouble scale;

            icon_width = gdk_pixbuf_get_width(icon);
            icon_height = gdk_pixbuf_get_height(icon);

            overlay_icon_width = ( (gdouble)notification_event->scale / 100. ) * (gdouble)icon_width;
            overlay_icon_height = ( (gdouble)notification_event->scale / 100. ) * (gdouble)icon_height;

            x = icon_width - overlay_icon_width;
            y = icon_height - overlay_icon_height;

            scale = (gdouble)overlay_icon_width / (gdouble)gdk_pixbuf_get_width(overlay_icon);

            gdk_pixbuf_composite(overlay_icon, icon,
                                 x, y,
                                 overlay_icon_width, overlay_icon_height,
                                 x, y,
                                 scale, scale,
                                 GDK_INTERP_BILINEAR, 255);

            g_object_unref(overlay_icon);
        }

        notify_notification_set_image_from_pixbuf(notification, icon);

        g_object_unref(icon);
    }
    else if ( overlay_icon != NULL )
    {
        notify_notification_set_image_from_pixbuf(notification, overlay_icon);

        g_object_unref(overlay_icon);
    }

    notify_notification_set_urgency(notification, NOTIFY_URGENCY_NORMAL);
    notify_notification_set_timeout(notification, notification_event->timeout);
    if ( ! notify_notification_show(notification, &error) )
        g_warning("Can't show the notification: %s", error->message);
    g_clear_error(&error);
    g_signal_connect(notification, "closed", G_CALLBACK(notification_closed_cb), NULL);
    g_object_unref(G_OBJECT(notification));
    g_free(message);
    g_free(title);

    return NULL;
}


static void
eventd_notification_config_init()
{
    events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)eventd_notification_event_free);
}

static void
eventd_notification_config_clean()
{
    g_hash_table_unref(events);
}

void
eventd_plugin_get_info(EventdPlugin *plugin)
{
    plugin->id = "notification";

    plugin->start = eventd_notification_start;
    plugin->stop = eventd_notification_stop;

    plugin->config_init = eventd_notification_config_init;
    plugin->config_clean = eventd_notification_config_clean;

    plugin->event_parse = eventd_notification_event_parse;
    plugin->event_action = eventd_notification_event_action;
}

