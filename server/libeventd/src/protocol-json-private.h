/*
 * libeventd-protocol-json - eventd JSON protocol library
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

#ifndef __EVENTD_PROTOCOL_JSON_PRIVATE_H__
#define __EVENTD_PROTOCOL_JSON_PRIVATE_H__

#include <yajl/yajl_common.h>

#include "protocol-private.h"

yajl_alloc_funcs eventd_protocol_json_alloc_funcs;

typedef enum {
    EVENTD_PROTOCOL_JSON_MESSAGE_TYPE_MISSING = 0,
    EVENTD_PROTOCOL_JSON_MESSAGE_TYPE_EVENT,
    EVENTD_PROTOCOL_JSON_MESSAGE_TYPE_SUBSCRIBE,
    EVENTD_PROTOCOL_JSON_MESSAGE_TYPE_UNKNOWN,
    _EVENTD_PROTOCOL_JSON_MESSAGE_TYPE_SIZE
} EventdProtocolJsonMessageType;

typedef enum {
    EVENTD_PROTOCOL_JSON_STATE_BASE = 0,
    EVENTD_PROTOCOL_JSON_STATE_MESSAGE,
    EVENTD_PROTOCOL_JSON_STATE_TYPE,
    EVENTD_PROTOCOL_JSON_STATE_UUID,
    EVENTD_PROTOCOL_JSON_STATE_CATEGORY,
    EVENTD_PROTOCOL_JSON_STATE_NAME,
    EVENTD_PROTOCOL_JSON_STATE_CATEGORIES,
    EVENTD_PROTOCOL_JSON_STATE_DATA,
} EventdProtocolJsonState;

typedef struct {
    EventdProtocol protocol;

    GError *error;
    struct {
        EventdProtocolJsonState state;
        EventdProtocolJsonMessageType type;
        gchar *uuid;
        gchar *category;
        gchar *name;
        GHashTable *categories;
        GHashTable *data;
        gchar *data_name;
    } message;
} EventdProtocolJson;

gboolean eventd_protocol_json_parse(EventdProtocol *protocol, const gchar *buffer, GError **error);
void eventd_protocol_json_parse_free(EventdProtocolJson *self);

gchar *eventd_protocol_json_generate_event(EventdProtocol *protocol, EventdEvent *event);

gchar *eventd_protocol_json_generate_subscribe(EventdProtocol *protocol, GHashTable *categories);
gchar *eventd_protocol_json_generate_bye(EventdProtocol *protocol, const gchar *message);


#endif /* __EVENTD_PROTOCOL_JSON_PRIVATE_H__ */
