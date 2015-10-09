/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2015 Quentin "Sardem FF7" Glidic
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

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libeventd-event.h>


static void
_eventd_nd_pixbuf_free_data(guchar *pixels, gpointer data)
{
    g_free(pixels);
}

GdkPixbuf *
eventd_nd_pixbuf_from_base64(EventdEvent *event, const gchar *name)
{
    GdkPixbuf *pixbuf = NULL;
    const gchar *base64;
    guchar *data;
    gsize length;
    const gchar *format;
    gchar *format_name;

    base64 = eventd_event_get_data(event, name);
    if ( base64 == NULL )
        return NULL;
    data = g_base64_decode(base64, &length);

    format_name = g_strconcat(name, "-format", NULL);
    format = eventd_event_get_data(event, format_name);
    g_free(format_name);

    if ( format != NULL )
    {
        gint width, height;
        gint stride;
        gboolean alpha;
        gchar *f;

        width = g_ascii_strtoll(format, &f, 16);
        height = g_ascii_strtoll(f+1, &f, 16);
        stride = g_ascii_strtoll(f+1, &f, 16);
        alpha = g_ascii_strtoll(f+1, &f, 16);

        pixbuf = gdk_pixbuf_new_from_data(data, GDK_COLORSPACE_RGB, alpha, 8, width, height, stride, _eventd_nd_pixbuf_free_data, NULL);
    }
    else
    {
        GError *error = NULL;
        GdkPixbufLoader *loader;

        loader = gdk_pixbuf_loader_new();

        if ( ! gdk_pixbuf_loader_write(loader, data, length, &error) )
        {
            g_warning("Couldn't write image data: %s", error->message);
            goto error;
        }

        if ( ! gdk_pixbuf_loader_close(loader, &error) )
        {
            g_warning("Couldn't load image data: %s", error->message);
            goto error;
        }

        pixbuf = g_object_ref(gdk_pixbuf_loader_get_pixbuf(loader));

    error:
        g_clear_error(&error);
        g_object_unref(loader);
        g_free(data);
    }

    return pixbuf;
}
