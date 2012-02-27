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

void eventd_nd_bubble_init(void);
void eventd_nd_bubble_uninit(void);

EventdNdBubble *eventd_nd_bubble_new(EventdNotificationNotification *notification, EventdNdStyle *style, GList *displays);
void eventd_nd_bubble_free(gpointer data);

void eventd_nd_bubble_show(EventdNdBubble *bubble);
void eventd_nd_bubble_hide(EventdNdBubble *bubble);

#endif /* __EVENTD_ND_NOTIFICATION_H__ */
