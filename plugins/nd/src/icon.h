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

#ifndef __EVENTD_ND_ICON_H__
#define __EVENTD_ND_ICON_H__

GdkPixbuf *eventd_nd_notification_contents_get_image(EventdNdNotificationContents *self);
GdkPixbuf *eventd_nd_notification_contents_get_icon(EventdNdNotificationContents *self);

void eventd_nd_cairo_image_and_icon_process(EventdNdNotificationContents *notification, EventdNdStyle *style, gint max_width, cairo_surface_t **image, cairo_surface_t **icon, gint *text_margin, gint *width, gint *height);
void eventd_nd_cairo_image_and_icon_draw(cairo_t *cr, cairo_surface_t *image, cairo_surface_t *icon, EventdNdStyle *style, gint width, gint height);

#endif /* __EVENTD_ND_ICON_H__ */
