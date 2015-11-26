/*
 * libeventd-event - Library to manipulate eventd events
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

#ifndef __EVENTD_PROTOCOL_H__
#define __EVENTD_PROTOCOL_H__

#include <libeventd-event.h>

G_BEGIN_DECLS

/*
 * EventdProtocol
 */

typedef struct _EventdProtocol               EventdProtocol;
typedef struct _EventdProtocolInterface      EventdProtocolInterface;

GType eventd_protocol_get_type(void);

#define EVENTD_TYPE_PROTOCOL                (eventd_protocol_get_type())
#define EVENTD_PROTOCOL(obj)                (G_TYPE_CHECK_INSTANCE_CAST((obj), EVENTD_TYPE_PROTOCOL, EventdProtocol))
#define EVENTD_IS_PROTOCOL(obj)             (G_TYPE_CHECK_INSTANCE_TYPE((obj), EVENTD_TYPE_PROTOCOL))
#define EVENTD_PROTOCOL_GET_INTERFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE((inst), EVENTD_TYPE_PROTOCOL, EventdProtocolInterface))

struct _EventdProtocolInterface
{
    GTypeInterface parent_iface;

    void (*add_event)(EventdProtocol *protocol, EventdEvent *event);
    void (*remove_event)(EventdProtocol *protocol, EventdEvent *event);

    gboolean (*parse)(EventdProtocol *protocol, gchar **buffer, GError **error);

    gchar *(*generate_event)(EventdProtocol *protocol, EventdEvent *event);
    gchar *(*generate_ended)(EventdProtocol *protocol, EventdEvent *event, EventdEventEndReason reason);

    gchar *(*generate_passive)(EventdProtocol *protocol);
    gchar *(*generate_subscribe)(EventdProtocol *protocol, GHashTable *categories);
    gchar *(*generate_bye)(EventdProtocol *protocol, const gchar *message);

    /* Signals */
    void (*event)(EventdProtocol *protocol, EventdEvent *event);
    void (*ended)(EventdProtocol *protocol, EventdEvent *event, EventdEventEndReason reason);

    void (*passive)(EventdProtocol *protocol);
    void (*subscribe)(EventdProtocol *protocol, GHashTable *categories);
    void (*bye)(EventdProtocol *protocol, const gchar *message);
};

/*
 * EventdProtocolEvp
 */
typedef struct _EventdProtocolEvp EventdProtocolEvp;
typedef struct _EventdProtocolEvpClass EventdProtocolEvpClass;
typedef struct _EventdProtocolEvpPrivate EventdProtocolEvpPrivate;

GType eventd_protocol_evp_get_type(void) G_GNUC_CONST;

#define EVENTD_TYPE_PROTOCOL_EVP            (eventd_protocol_evp_get_type())
#define EVENTD_PROTOCOL_EVP(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), EVENTD_TYPE_PROTOCOL_EVP, EventdProtocolEvp))
#define EVENTD_IS_PROTOCOL_EVP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), EVENTD_TYPE_PROTOCOL_EVP))
#define EVENTD_PROTOCOL_EVP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), EVENTD_TYPE_PROTOCOL_EVP, EventdProtocolEvpClass))
#define EVENTD_IS_PROTOCOL_EVP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), EVENTD_TYPE_PROTOCOL_EVP))
#define EVENTD_PROTOCOL_EVP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EVENTD_TYPE_PROTOCOL_EVP, EventdProtocolEvpClass))

struct _EventdProtocolEvp
{
    GObject parent_object;

    /*< private >*/
    EventdProtocolEvpPrivate *priv;
};

struct _EventdProtocolEvpClass
{
    GObjectClass parent_class;
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

EventdProtocol *eventd_protocol_evp_new(void);

void eventd_protocol_add_event(EventdProtocol *protocol, EventdEvent *event);
void eventd_protocol_remove_event(EventdProtocol *protocol, EventdEvent *event);

/*
 * Generator
 */


gboolean eventd_protocol_parse(EventdProtocol *protocol, gchar **buffer, GError **error);

gchar *eventd_protocol_generate_event(EventdProtocol *protocol, EventdEvent *event);
gchar *eventd_protocol_generate_ended(EventdProtocol *protocol, EventdEvent *event, EventdEventEndReason reason);

gchar *eventd_protocol_generate_passive(EventdProtocol *protocol);
gchar *eventd_protocol_generate_subscribe(EventdProtocol *protocol, GHashTable *categories);
gchar *eventd_protocol_generate_bye(EventdProtocol *protocol, const gchar *message);

G_END_DECLS

#endif /* __EVENTD_PROTOCOL_H__ */
