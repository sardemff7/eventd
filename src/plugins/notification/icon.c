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

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <eventd-plugin.h>
#include <plugin-helper.h>

#include "notification.h"
#include "icon.h"

static GdkPixbuf *
eventd_notification_get_pixbuf_from_data(const char *icon_name, GHashTable *event_data)
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

static gpointer
eventd_notification_icon_get_pixbuf_from_file(const gchar *filename, GHashTable *event_data)
{
    gchar *real_filename;
    GError *error = NULL;
    GdkPixbuf *ret = NULL;

    real_filename = eventd_plugin_helper_regex_replace_event_data(filename, event_data, NULL);

    if ( ! g_path_is_absolute(filename) )
    {
        gchar *tmp;
        tmp = real_filename;
        real_filename = g_build_filename(g_get_user_data_dir(), PACKAGE_NAME, "icons", tmp, NULL);
        g_free(tmp);
    }

    if ( ! g_file_test(real_filename, G_FILE_TEST_IS_REGULAR) )
        goto fail;

    if ( ( ret = gdk_pixbuf_new_from_file(real_filename, &error) ) == NULL )
        g_warning("Couldn’t load icon file '%s': %s", real_filename, error->message);
    g_clear_error(&error);

fail:
    g_free(real_filename);
    return ret;
}

void
eventd_notification_icon_get_pixbuf(EventdEvent *event, EventdNotificationEvent *notification_event, EventdNotificationNotification *notification)
{
    if ( g_str_has_prefix(notification_event->icon, "file://") )
        notification->icon = eventd_notification_icon_get_pixbuf_from_file(notification_event->icon+7, event->data);
    else
        notification->icon = eventd_notification_get_pixbuf_from_data(notification_event->icon, event->data);
    if ( g_str_has_prefix(notification_event->overlay_icon, "file://") )
        notification->overlay_icon = eventd_notification_icon_get_pixbuf_from_file(notification_event->overlay_icon+7, event->data);
    else
        notification->overlay_icon = eventd_notification_get_pixbuf_from_data(notification_event->overlay_icon, event->data);
    if ( ( notification->icon != NULL ) && ( notification->overlay_icon != NULL ) )
    {
        gint icon_width, icon_height;
        gint overlay_icon_width, overlay_icon_height;
        gint x, y;
        gdouble scale;

        notification->merged_icon = gdk_pixbuf_copy(notification->icon);

        icon_width = gdk_pixbuf_get_width(notification->merged_icon);
        icon_height = gdk_pixbuf_get_height(notification->merged_icon);

        overlay_icon_width = notification_event->scale * (gdouble)icon_width;
        overlay_icon_height = notification_event->scale * (gdouble)icon_height;

        x = icon_width - overlay_icon_width;
        y = icon_height - overlay_icon_height;

        scale = (gdouble)overlay_icon_width / (gdouble)gdk_pixbuf_get_width(notification->overlay_icon);

        gdk_pixbuf_composite(notification->overlay_icon, notification->merged_icon,
                             x, y,
                             overlay_icon_width, overlay_icon_height,
                             x, y,
                             scale, scale,
                             GDK_INTERP_BILINEAR, 255);
    }
}

gchar *
eventd_notification_icon_get_base64(gpointer icon)
{
    gchar *data;
    gsize data_length;
    gchar *base64;

    gdk_pixbuf_save_to_buffer(icon, &data, &data_length, "png", NULL, NULL);
    g_object_unref(icon);

    base64 = g_base64_encode(data, data_length);
    g_free(data);

    return base64;
}

void
eventd_notification_icon_unref(gpointer icon)
{
    g_object_unref(icon);
}
