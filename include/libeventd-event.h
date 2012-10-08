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

#ifndef __EVENTD_EVENT_H__
#define __EVENTD_EVENT_H__

#include <libeventd-event-types.h>

G_BEGIN_DECLS

GType eventd_event_get_type(void);
GType eventd_event_end_reason_get_type(void);

#define EVENTD_TYPE_EVENT            (eventd_event_get_type())
#define EVENTD_EVENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), EVENTD_TYPE_EVENT, EventdEvent))
#define EVENTD_IS_EVENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), EVENTD_TYPE_EVENT))
#define EVENTD_EVENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), EVENTD_TYPE_EVENT, EventdEventClass))
#define EVENTD_IS_EVENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), EVENTD_TYPE_EVENT))
#define EVENTD_EVENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EVENTD_TYPE_EVENT, EventdEventClass))

#define EVENTD_TYPE_EVENT_END_REASON (eventd_event_end_reason_get_type())


struct _EventdEvent
{
        GObject parent_object;

        /*< private >*/
        EventdEventPrivate *priv;
};

struct _EventdEventClass
{
        GObjectClass parent_class;

        /* Signals */
        void (*updated)  (EventdEvent *event);
        void (*answered) (EventdEvent *event, const gchar *answer);
        void (*ended)    (EventdEvent *event, EventdEventEndReason reason);
};

EventdEvent *eventd_event_new(const gchar *name);

void eventd_event_update(EventdEvent *event, const gchar *name);
void eventd_event_answer(EventdEvent *event, const gchar *answer);
void eventd_event_end(EventdEvent *event, EventdEventEndReason reason);


void eventd_event_set_category(EventdEvent *event, const gchar *category);
void eventd_event_set_timeout(EventdEvent *event, gint64 timeout);
void eventd_event_add_data(EventdEvent *event, gchar *name, gchar *content);
void eventd_event_add_answer(EventdEvent *event, const gchar *name);
void eventd_event_add_answer_data(EventdEvent *event, gchar *name, gchar *content);

const gchar *eventd_event_get_category(EventdEvent *event);
const gchar *eventd_event_get_name(EventdEvent *event);
gint64 eventd_event_get_timeout(EventdEvent *event);
const gchar *eventd_event_get_data(EventdEvent *event, const gchar *name);
const gchar *eventd_event_get_answer_data(EventdEvent *event, const gchar *name);

G_END_DECLS

#endif /* __EVENTD_EVENT_H__ */
