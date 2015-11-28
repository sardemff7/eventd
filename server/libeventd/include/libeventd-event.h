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

#ifndef __EVENTD_EVENT_H__
#define __EVENTD_EVENT_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

/*
 * EventdEvent
 */
typedef struct _EventdEvent EventdEvent;

GType eventd_event_get_type(void) G_GNUC_CONST;

#define EVENTD_TYPE_EVENT            (eventd_event_get_type())

/*
 * Functions
 */
EventdEvent *eventd_event_new(const gchar *category, const gchar *name);
EventdEvent *eventd_event_ref(EventdEvent *event);
void eventd_event_unref(EventdEvent *event);

void eventd_event_add_data(EventdEvent *event, gchar *name, gchar *content);

const gchar *eventd_event_get_uuid(EventdEvent *event);
const gchar *eventd_event_get_category(const EventdEvent *event);
const gchar *eventd_event_get_name(const EventdEvent *event);
gboolean eventd_event_has_data(const EventdEvent *event, const gchar *name);
const gchar *eventd_event_get_data(const EventdEvent *event, const gchar *name);
GHashTable *eventd_event_get_all_data(const EventdEvent *event);

G_END_DECLS

#endif /* __EVENTD_EVENT_H__ */
