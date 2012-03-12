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

#ifndef __EVENTD_ND_NOTIFICATION_H__
#define __EVENTD_ND_NOTIFICATION_H__

#include <libeventd-event-types.h>

typedef struct _EventdNdNotification EventdNdNotification;

void eventd_nd_notification_init();
void eventd_nd_notification_uninit();

EventdNdNotification *eventd_nd_notification_new(EventdEvent *event, const gchar *title, const gchar *message, const gchar *icon_name, const gchar *overlay_icon_name);
void eventd_nd_notification_free(EventdNdNotification *notification);

const gchar *eventd_nd_notification_get_title(EventdNdNotification *notification);
const gchar *eventd_nd_notification_get_message(EventdNdNotification *notification);
void eventd_nd_notification_get_image(EventdNdNotification *notification, const guchar **data, gsize *length, const gchar **format);
void eventd_nd_notification_get_icon(EventdNdNotification *notification, const guchar **data, gsize *length, const gchar **format);

#endif /* __EVENTD_ND_NOTIFICATION_H__ */
