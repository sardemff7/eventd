/*
 * libeventd - Internal helper
 *
 * Copyright © 2011 Quentin "Sardem FF7" Glidic
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

#ifndef __LIBEVENTD_EVENT_H__
#define __LIBEVENTD_EVENT_H__

typedef struct _EventdEvent EventdEvent;

EventdEvent *libeventd_event_new(const gchar *type);
void libeventd_event_ref(EventdEvent *event);
void libeventd_event_unref(EventdEvent *event);

void libeventd_event_add_data(EventdEvent *event, gchar *name, gchar *content);

const gchar *libeventd_event_get_type(EventdEvent *event);
GHashTable *libeventd_event_get_data(EventdEvent *event);

#endif /* __LIBEVENTD_EVENT_H__ */
