/*
 * libeventd-event - Library to manipulate eventd events
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

#ifndef __EVENTD_PROTOCOL_H__
#define __EVENTD_PROTOCOL_H__

#include <libeventd-event.h>

G_BEGIN_DECLS

/*
 * EventdProtocol
 */

typedef struct _EventdProtocol               EventdProtocol;
typedef struct _EventdProtocolCallbacks      EventdProtocolCallbacks;

GType eventd_protocol_get_type(void);

#define EVENTD_TYPE_PROTOCOL                (eventd_protocol_get_type())

struct _EventdProtocolCallbacks
{
    void (*event)(EventdProtocol *protocol, EventdEvent *event, gpointer user_data);
    void (*subscribe)(EventdProtocol *protocol, GHashTable *categories, gpointer user_data);
    void (*bye)(EventdProtocol *protocol, const gchar *message, gpointer user_data);
};

/*
 * EventdProtocolParseError
 */
typedef enum {
    EVENTD_PROTOCOL_PARSE_ERROR_UNEXPECTED_TOKEN,
    EVENTD_PROTOCOL_PARSE_ERROR_MALFORMED,
    EVENTD_PROTOCOL_PARSE_ERROR_GARBAGE,
    EVENTD_PROTOCOL_PARSE_ERROR_WRONG_UUID,
    EVENTD_PROTOCOL_PARSE_ERROR_KNOWN_ID,
    EVENTD_PROTOCOL_PARSE_ERROR_UNKNOWN_ID,
    EVENTD_PROTOCOL_PARSE_ERROR_UNKNOWN
} EventdProtocolParseError;

GQuark eventd_protocol_parse_error_quark(void);
#define EVENTD_PROTOCOL_PARSE_ERROR (eventd_protocol_parse_error_quark())

/*
 * Functions
 */

EventdProtocol *eventd_protocol_new(const EventdProtocolCallbacks *callbacks, gpointer user_data, GDestroyNotify notify);
EventdProtocol *eventd_protocol_ref(EventdProtocol *protocol);
void eventd_protocol_unref(EventdProtocol *protocol);

gboolean eventd_protocol_parse(EventdProtocol *protocol, const gchar *buffer, GError **error);

gchar *eventd_protocol_generate_event(EventdProtocol *protocol, EventdEvent *event);
gchar *eventd_protocol_generate_subscribe(EventdProtocol *protocol, GHashTable *categories);
gchar *eventd_protocol_generate_bye(EventdProtocol *protocol, const gchar *message);


G_END_DECLS

#endif /* __EVENTD_PROTOCOL_H__ */
