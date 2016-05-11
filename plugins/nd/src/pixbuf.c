/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2016 Quentin "Sardem FF7" Glidic
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <glib.h>
#include <glib-object.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libeventd-event.h>


static GdkPixbuf *
_eventd_nd_from_file(const gchar *path, gint width, gint height)
{
    GError *error = NULL;
    GdkPixbufFormat *format;
    GdkPixbuf *pixbuf;

    if ( *path == 0 )
        return NULL;

    if ( ( ( width > 0 ) || ( height > 0 ) ) && ( ( format = gdk_pixbuf_get_file_info(path, NULL, NULL) ) != NULL ) && gdk_pixbuf_format_is_scalable(format) )
        pixbuf = gdk_pixbuf_new_from_file_at_size(path, width, height, &error);
    else
        pixbuf = gdk_pixbuf_new_from_file(path, &error);

    if ( pixbuf == NULL )
        g_warning("Couldn't load file '%s': %s", path, error->message);
    g_clear_error(&error);

    return pixbuf;
}

static void
_eventd_nd_pixbuf_free_data(guchar *pixels, gpointer data)
{
    g_free(pixels);
}

static GdkPixbuf *
_eventd_nd_pixbuf_from_base64(gchar *uri, gint width, gint height)
{
    GdkPixbuf *pixbuf = NULL;
    gchar *c;
    gchar *mime_type = uri;
    guchar *data;
    gsize length;

    /* We checked for ";base64," already */
    c = g_utf8_strchr(mime_type, -1, ',');
    *c = '\0';

    data = g_base64_decode(c + 1, &length);

    c = g_utf8_strchr(mime_type, c - mime_type, ';');
    *c++ = '\0';

    if ( ( g_strcmp0(mime_type, "image/x.eventd.gdkpixbuf") == 0 ) && g_str_has_prefix(c, "format=") )
    {
        const gchar *format = c + strlen("format=");
        gint width, height;
        gint stride;
        gboolean alpha;
        gchar *f;

        width = g_ascii_strtoll(format, &f, 16);
        height = g_ascii_strtoll(f+1, &f, 16);
        stride = g_ascii_strtoll(f+1, &f, 16);
        alpha = g_ascii_strtoll(f+1, &f, 16);

        return gdk_pixbuf_new_from_data(data, GDK_COLORSPACE_RGB, alpha, 8, width, height, stride, _eventd_nd_pixbuf_free_data, NULL);
    }

    GError *error = NULL;
    GdkPixbufLoader *loader;

    if ( *mime_type != '\0' )
    {
        loader = gdk_pixbuf_loader_new_with_mime_type(mime_type, &error);
        if ( loader == NULL )
        {
            g_warning("Couldn't create loader for MIME type '%s': %s", mime_type, error->message);
            goto end;
        }
        GdkPixbufFormat *format;
        if ( ( ( width > 0 ) || ( height > 0 ) ) && ( ( format = gdk_pixbuf_loader_get_format(loader) ) != NULL ) && gdk_pixbuf_format_is_scalable(format) )
            gdk_pixbuf_loader_set_size(loader, width, height);
    }
    else
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
    g_object_unref(loader);
end:
    g_clear_error(&error);
    g_free(data);
    return pixbuf;
}

GdkPixbuf *
eventd_nd_pixbuf_from_uri(gchar *uri, gint width, gint height)
{
    GdkPixbuf *pixbuf = NULL;
    if ( g_str_has_prefix(uri, "file://") )
        pixbuf = _eventd_nd_from_file(uri + strlen("file://"), width, height);
    else if ( g_str_has_prefix(uri, "data:") )
        pixbuf = _eventd_nd_pixbuf_from_base64(uri + strlen("data:"), width, height);
    g_free(uri);

    return pixbuf;
}
