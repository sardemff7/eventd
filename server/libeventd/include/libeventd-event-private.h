/*
 * libeventd-event - Main eventd library - Event manipulation
 *
 * Copyright © 2011-2015 Quentin "Sardem FF7" Glidic
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with eventd. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __EVENTD_EVENT_PRIVATE_H__
#define __EVENTD_EVENT_PRIVATE_H__

#define UUID_STR_SIZE 37 /* 36 + \0 */

EventdEvent *eventd_event_new_for_uuid(uuid_t uuid, const gchar *category, const gchar *name);
const gchar *eventd_event_get_uuid(EventdEvent *event);

void eventd_event_set_all_data(EventdEvent *event, GHashTable *data);
GList *eventd_event_get_answers(EventdEvent *event);
void eventd_event_set_all_answer_data(EventdEvent *event, GHashTable *data);

G_END_DECLS

#endif /* __EVENTD_EVENT_PRIVATE_H__ */
