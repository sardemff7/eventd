/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2017 Quentin "Sardem FF7" Glidic
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

#include "config.h"

#include <string.h>

#include <glib.h>
#include <glib-object.h>

#include <nkutils-xdg-theme.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "libeventd-event.h"


static GdkPixbuf *
_eventd_nd_pixbuf_from_file(const gchar *path, gint width, gint height)
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

GdkPixbuf *
eventd_nd_pixbuf_from_uri(gchar *uri, gint width, gint height, gint scale)
{
    GdkPixbuf *pixbuf = NULL;
    if ( g_str_has_prefix(uri, "file://") )
        pixbuf = _eventd_nd_pixbuf_from_file(uri + strlen("file://"), width * scale, height * scale);
    g_free(uri);

    return pixbuf;
}

static void
_eventd_nd_pixbuf_free_data(guchar *pixels, gpointer data)
{
    g_variant_unref(data);
}

GdkPixbuf *
eventd_nd_pixbuf_from_data(GVariant *var, gint width, gint height, gint scale)
{
    GdkPixbuf *pixbuf = NULL;
    GError *error = NULL;
    const gchar *mime_type;
    GVariant *invar;

    g_variant_get(var, "(m&sm&sv)", &mime_type, NULL, &invar);

    if ( g_strcmp0(mime_type, "image/x.eventd.pixbuf") == 0 )
    {
        gboolean a;
        gint b, w, h, s, n;
        GVariant *data;
        g_variant_get(invar, "(iiibii@ay)", &w, &h, &s, &a, &b, &n, &data);
         /* This is the only format gdk-pixbuf can read */
        if ( ( b == 8 ) && ( n == ( a ? 4 : 3 ) ) && ( h * s == (gint) g_variant_get_size(data) ) )
            return gdk_pixbuf_new_from_data(g_variant_get_data(data), GDK_COLORSPACE_RGB, a, b, w, h, s, _eventd_nd_pixbuf_free_data, data);
        g_variant_unref(data);
        goto end;
    }

    if ( ! g_variant_is_of_type(invar, G_VARIANT_TYPE_BYTESTRING) )
        goto end;

    GdkPixbufLoader *loader;
    const guchar *data;
    gsize length;

    data = g_variant_get_data(invar);
    length = g_variant_get_size(invar);

    if ( mime_type != NULL )
    {
        loader = gdk_pixbuf_loader_new_with_mime_type(mime_type, &error);
        if ( loader == NULL )
        {
            g_warning("Couldn't create loader for MIME type '%s': %s", mime_type, error->message);
            goto end;
        }
        GdkPixbufFormat *format;
        if ( ( ( width > 0 ) || ( height > 0 ) ) && ( ( format = gdk_pixbuf_loader_get_format(loader) ) != NULL ) && gdk_pixbuf_format_is_scalable(format) )
            gdk_pixbuf_loader_set_size(loader, width * scale, height * scale);
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
    g_variant_unref(invar);
    g_variant_unref(var);
    g_clear_error(&error);
    return pixbuf;
}

GdkPixbuf *
eventd_nd_pixbuf_from_theme(NkXdgThemeContext *context, gchar *uri, gint size, gint scale)
{
    GdkPixbuf *pixbuf = NULL;
    const gchar *theme = NULL;
    const gchar *name = uri + strlen("theme:");
    gchar *file;

    gchar *c;
    if ( ( c = g_utf8_strchr(name, -1, '/') ) != NULL )
    {
        *c = '\0';
        theme = name;
        name = ++c;
    }

    file = nk_xdg_theme_get_icon(context, theme, NULL, name, size, scale, TRUE);
    if ( file != NULL )
        pixbuf = _eventd_nd_pixbuf_from_file(file, size * scale, size * scale);
    g_free(file);
    g_free(uri);

    return pixbuf;
}
