/*
 * libeventd-protocol - Main eventd library - Protocol manipulation
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

#ifndef __EVENTD_EVENT_EVENT_PRIVATE_H__
#define __EVENTD_EVENT_EVENT_PRIVATE_H__

struct _EventdEventPrivate {
    NkUuid uuid;
    gchar *category;
    gchar *name;
    gint64 timeout;
    GHashTable *data;
    GList *answers;
    GHashTable *answer_data;
};

EventdEvent *eventd_event_new_for_uuid(NkUuid uuid, const gchar *category, const gchar *name);

#endif /* __EVENTD_EVENT_EVENT_PRIVATE_H__ */
