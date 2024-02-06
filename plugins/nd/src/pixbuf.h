/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2024 Morgane "Sardem FF7" Glidic
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

#ifndef __EVENTD_ND_PIXBUF_H__
#define __EVENTD_ND_PIXBUF_H__

#if ( ( GDK_PIXBUF_MAJOR < 2 ) || ( ( GDK_PIXBUF_MAJOR == 2 ) && ( GDK_PIXBUF_MINOR < 31 ) ) )
static inline const guchar *gdk_pixbuf_read_pixels(GdkPixbuf *pixbuf) { return gdk_pixbuf_get_pixels(pixbuf); }
#endif /* gdk-pixbux < 2.32 */

GdkPixbuf *eventd_nd_pixbuf_from_uri(gchar *uri, gint width, gint height, gint scale);
GdkPixbuf *eventd_nd_pixbuf_from_data(GVariant *data, gint width, gint height, gint scale);
GdkPixbuf *eventd_nd_pixbuf_from_theme(NkXdgThemeContext *context, const gchar *theme, gchar *uri, gint size, gint scale);

#endif /* __EVENTD_ND_PIXBUF_H__ */
