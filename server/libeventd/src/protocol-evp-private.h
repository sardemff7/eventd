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

#ifndef __EVENTD_PROTOCOL_EVP_PRIVATE_H__
#define __EVENTD_PROTOCOL_EVP_PRIVATE_H__

typedef enum {
    EVENTD_PROTOCOL_EVP_STATE_BASE = 0,
    EVENTD_PROTOCOL_EVP_STATE_PASSIVE,
    EVENTD_PROTOCOL_EVP_STATE_BYE,
    EVENTD_PROTOCOL_EVP_STATE_DOT_DATA,
    EVENTD_PROTOCOL_EVP_STATE_DOT_EVENT,
    EVENTD_PROTOCOL_EVP_STATE_DOT_ANSWERED,
    EVENTD_PROTOCOL_EVP_STATE_IGNORING,
    _EVENTD_PROTOCOL_EVP_STATE_SIZE
} EventdProtocolEvpState;

struct _EventdProtocolEvpPrivate {
    GHashTable *events;
    EventdProtocolEvpState state;
    EventdProtocolEvpState base_state;
    union {
        struct {
            guint64 level;
        } catchall;
        EventdEvent *event;
        struct {
            EventdEvent *event;
            gchar *answer;
        } answer;
    };
    struct {
        GHashTable *hash;
        EventdProtocolEvpState return_state;
        gchar *name;
        GString *value;
    } data;
};

void eventd_protocol_evp_add_event(EventdProtocolEvp *self, EventdEvent *event);
void eventd_protocol_evp_remove_event(EventdProtocolEvp *self, EventdEvent *event);

gboolean eventd_protocol_evp_parse(EventdProtocol *protocol, gchar **buffer, GError **error);
void eventd_protocol_evp_parse_free(EventdProtocolEvp *self);

gchar *eventd_protocol_evp_generate_event(EventdProtocol *protocol, EventdEvent *event);
gchar *eventd_protocol_evp_generate_answered(EventdProtocol *protocol, EventdEvent *event, const gchar *answer);
gchar *eventd_protocol_evp_generate_ended(EventdProtocol *protocol, EventdEvent *event, EventdEventEndReason reason);

gchar *eventd_protocol_evp_generate_passive(EventdProtocol *protocol);
gchar *eventd_protocol_evp_generate_bye(EventdProtocol *protocol, const gchar *message);


#endif /* __EVENTD_PROTOCOL_EVP_PRIVATE_H__ */
