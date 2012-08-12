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

#ifndef __LIBEVENTD_ND_NOTIFICATION_H__
#define __LIBEVENTD_ND_NOTIFICATION_H__

#include <libeventd-event-types.h>

typedef struct _LibeventdNdNotification LibeventdNdNotification;

void libeventd_nd_notification_init();
void libeventd_nd_notification_uninit();

LibeventdNdNotification *libeventd_nd_notification_new(EventdEvent *event, const gchar *title, const gchar *message, const gchar *icon_name, const gchar *overlay_icon_name, gint width, gint height);
void libeventd_nd_notification_free(LibeventdNdNotification *notification);

const gchar *libeventd_nd_notification_get_title(LibeventdNdNotification *notification);
const gchar *libeventd_nd_notification_get_message(LibeventdNdNotification *notification);
GdkPixbuf *libeventd_nd_notification_get_image(LibeventdNdNotification *self);
GdkPixbuf *libeventd_nd_notification_get_icon(LibeventdNdNotification *self);

#endif /* __LIBEVENTD_ND_NOTIFICATION_H__ */
