/*
 * libeventd-event - Library to manipulate eventd events
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

#ifndef __EVENTD_EVENT_H__
#define __EVENTD_EVENT_H__

G_BEGIN_DECLS

typedef struct _EventdEvent EventdEvent;
typedef struct _EventdEventClass EventdEventClass;
typedef struct _EventdEventPrivate EventdEventPrivate;

struct _EventdEvent
{
        GObject parent_object;

        /*< private >*/
        EventdEventPrivate *priv;
};

struct _EventdEventClass
{
        GObjectClass parent_class;
};

EventdEvent *eventd_event_new(const gchar *type);

void eventd_event_add_data(EventdEvent *event, gchar *name, gchar *content);
void eventd_event_add_pong_data(EventdEvent *event, gchar *name, gchar *content);

const gchar *eventd_event_get_type(EventdEvent *event);
GHashTable *eventd_event_get_data(EventdEvent *event);
GHashTable *eventd_event_get_pong_data(EventdEvent *event);

G_END_DECLS

#endif /* __EVENTD_EVENT_H__ */
