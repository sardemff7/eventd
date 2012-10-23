/*
 * libeventd-event - Library to manipulate eventd events
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

#ifndef __EVENTD_EVENT_PRIVATE_H__
#define __EVENTD_EVENT_PRIVATE_H__

#include <libeventd-event.h>

G_BEGIN_DECLS

void eventd_event_set_all_data(EventdEvent *event, GHashTable *data);
void eventd_event_set_all_answer_data(EventdEvent *event, GHashTable *data);
GHashTable *eventd_event_get_all_data(EventdEvent *event);
GList *eventd_event_get_answers(EventdEvent *event);
GHashTable *eventd_event_get_all_answer_data(EventdEvent *event);

G_END_DECLS

#endif /* __EVENTD_EVENT_PRIVATE_H__ */
