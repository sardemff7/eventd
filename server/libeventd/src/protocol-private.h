/*
 * libeventd-protocol - Main eventd library - Protocol manipulation
 *
 * Copyright © 2011-2016 Quentin "Sardem FF7" Glidic
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

#ifndef __EVENTD_PROTOCOL_PRIVATE_H__
#define __EVENTD_PROTOCOL_PRIVATE_H__

struct _EventdProtocol {
    guint64 refcount;

    const EventdProtocolCallbacks *callbacks;
    gpointer user_data;
    GDestroyNotify notify;

    void (*free)(EventdProtocol *protocol);

    gboolean (*parse)(EventdProtocol *protocol, const gchar *buffer, GError **error);

    gchar *(*generate_event)(EventdProtocol *protocol, EventdEvent *event);
    gchar *(*generate_passive)(EventdProtocol *protocol);
    gchar *(*generate_subscribe)(EventdProtocol *protocol, GHashTable *categories);
    gchar *(*generate_bye)(EventdProtocol *protocol, const gchar *message);

};

static inline EventdProtocol *
eventd_protocol_new(gsize size, const EventdProtocolCallbacks *callbacks, gpointer user_data, GDestroyNotify notify)
{
    EventdProtocol *self;

    self = g_malloc0_n(1, size);
    self->refcount = 1;

    self->callbacks = callbacks;
    self->user_data = user_data;
    self->notify = notify;

    return self;
}

static inline void
eventd_protocol_call_event(EventdProtocol *self, EventdEvent *event)
{
    if ( self->callbacks->event != NULL )
        self->callbacks->event(self, event, self->user_data);
}

static inline void
eventd_protocol_call_passive(EventdProtocol *self)
{
    if ( self->callbacks->passive != NULL )
        self->callbacks->passive(self, self->user_data);
}

static inline void
eventd_protocol_call_subscribe(EventdProtocol *self, GHashTable *subscriptions)
{
    if ( self->callbacks->subscribe != NULL )
        self->callbacks->subscribe(self, subscriptions, self->user_data);
}

static inline void
eventd_protocol_call_bye(EventdProtocol *self, const gchar *message)
{
    if ( self->callbacks->bye != NULL )
        self->callbacks->bye(self, message, self->user_data);
}

#endif /* __EVENTD_PROTOCOL_PRIVATE_H__ */
