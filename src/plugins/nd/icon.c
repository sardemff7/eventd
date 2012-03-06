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

#if ENABLE_GDK_PIXBUF

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "icon.h"

GdkPixbuf *
eventd_notification_icon_get_pixbuf_from_data(const guchar *data, gsize length, guint size)
{
    GError *error = NULL;
    GdkPixbufLoader *loader;
    GdkPixbuf *pixbuf = NULL;

    if ( data == NULL )
        return NULL;

    loader = gdk_pixbuf_loader_new();

    if ( size != 0 )
        gdk_pixbuf_loader_set_size(loader, size, size);

    if ( ! gdk_pixbuf_loader_write(loader, data, length, &error) )
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
    g_object_unref(loader);
    return pixbuf;
}

#endif /* ENABLE_GDK_PIXBUF */
