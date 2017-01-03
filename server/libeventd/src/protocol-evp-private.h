/*
 * libeventd-protocol - Main eventd library - Protocol manipulation
 *
 * Copyright © 2011-2017 Quentin "Sardem FF7" Glidic
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

#ifndef __EVENTD_PROTOCOL_EVP_PRIVATE_H__
#define __EVENTD_PROTOCOL_EVP_PRIVATE_H__

typedef enum {
    EVENTD_PROTOCOL_EVP_STATE_BASE = 0,
    EVENTD_PROTOCOL_EVP_STATE_PASSIVE,
    EVENTD_PROTOCOL_EVP_STATE_SUBSCRIBE,
    EVENTD_PROTOCOL_EVP_STATE_BYE,
    EVENTD_PROTOCOL_EVP_STATE_DOT_SUBSCRIBE,
    EVENTD_PROTOCOL_EVP_STATE_DOT_EVENT,
    EVENTD_PROTOCOL_EVP_STATE_IGNORING,
    _EVENTD_PROTOCOL_EVP_STATE_SIZE
} EventdProtocolState;

struct _EventdProtocol {
    guint64 refcount;

    const EventdProtocolCallbacks *callbacks;
    gpointer user_data;
    GDestroyNotify notify;

    EventdProtocolState state;
    EventdProtocolState base_state;
    union {
        struct {
            guint64 level;
        } catchall;
        EventdEvent *event;
        GHashTable *subscriptions;
    };
    struct {
        GHashTable *hash;
        EventdProtocolState return_state;
        gchar *name;
        GString *value;
    } data;
};

gboolean eventd_protocol_evp_parse(EventdProtocol *protocol, const gchar *buffer, GError **error);
void eventd_protocol_evp_parse_free(EventdProtocol *self);

gchar *eventd_protocol_evp_generate_event(EventdProtocol *protocol, EventdEvent *event);
gchar *eventd_protocol_evp_generate_subscribe(EventdProtocol *protocol, GHashTable *categories);
gchar *eventd_protocol_evp_generate_bye(EventdProtocol *protocol, const gchar *message);


#endif /* __EVENTD_PROTOCOL_EVP_PRIVATE_H__ */
