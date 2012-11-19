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
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libeventd-event.h>
#include <libeventd-config.h>

#include "image.h"

static GdkPixbuf *
_eventd_libnotify_icon_get_pixbuf_from_file(const gchar *filename)
{
    GError *error = NULL;
    GdkPixbuf *pixbuf;

    pixbuf = gdk_pixbuf_new_from_file(filename, &error);
    if ( pixbuf == NULL )
    {
        g_warning("Couldn't load icon file: %s", error->message);
        g_clear_error(&error);
    }

    return pixbuf;
}

static GdkPixbuf *
_eventd_libnotify_icon_get_pixbuf_from_base64(const gchar *base64)
{
    GError *error = NULL;
    guchar *data;
    gsize length;
    GdkPixbufLoader *loader;
    GdkPixbuf *pixbuf = NULL;

    if ( base64 == NULL )
        return NULL;

    data = g_base64_decode(base64, &length);

    loader = gdk_pixbuf_loader_new();

    if ( ! gdk_pixbuf_loader_write(loader, data, length, &error) )
    {
        g_warning("Couldn't write icon data: %s", error->message);
        g_clear_error(&error);
        goto fail;
    }

    if ( ! gdk_pixbuf_loader_close(loader, &error) )
    {
        g_warning("Couldn't terminate icon data loading: %s", error->message);
        g_clear_error(&error);
        goto fail;
    }

    pixbuf = g_object_ref(gdk_pixbuf_loader_get_pixbuf(loader));

fail:
    g_free(data);
    g_object_unref(loader);
    return pixbuf;
}

GdkPixbuf *
eventd_libnotify_get_image(EventdEvent *event, const gchar *image_name, const gchar *icon_name, gdouble overlay_scale, gchar **icon_uri)
{
    gchar *file;
    GdkPixbuf *image;
    GdkPixbuf *icon;

    if ( ( file = libeventd_config_get_filename(image_name, event, "icons") ) != NULL )
        image = _eventd_libnotify_icon_get_pixbuf_from_file(file);
    else
        image = _eventd_libnotify_icon_get_pixbuf_from_base64(eventd_event_get_data(event, image_name));
    g_free(file);

    if ( ( file = libeventd_config_get_filename(icon_name, event, "icons") ) != NULL )
    {
        *icon_uri = g_strconcat("file://", file, NULL);
        icon = _eventd_libnotify_icon_get_pixbuf_from_file(file);
    }
    else
        icon = _eventd_libnotify_icon_get_pixbuf_from_base64(eventd_event_get_data(event, icon_name));
    g_free(file);

    if ( ( image != NULL ) && ( icon != NULL ) )
    {
        gint image_width, image_height;
        gint icon_width, icon_height;
        gint x, y;
        gdouble scale;

        image_width = gdk_pixbuf_get_width(image);
        image_height = gdk_pixbuf_get_height(image);

        icon_width = overlay_scale * (gdouble) image_width;
        icon_height = overlay_scale * (gdouble) image_height;

        x = image_width - icon_width;
        y = image_height - icon_height;

        scale = (gdouble) icon_width / (gdouble) gdk_pixbuf_get_width(icon);

        gdk_pixbuf_composite(icon, image,
                             x, y,
                             icon_width, icon_height,
                             x, y,
                             scale, scale,
                             GDK_INTERP_BILINEAR, 255);

        g_object_unref(icon);
    }
    else if ( ( image == NULL ) && ( icon != NULL ) )
        image = icon;

    return image;
}
