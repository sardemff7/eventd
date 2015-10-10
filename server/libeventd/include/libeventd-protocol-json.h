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

#ifndef __EVENTD_PROTOCOL_JSON_H__
#define __EVENTD_PROTOCOL_JSON_H__

#include <libeventd-protocol.h>

G_BEGIN_DECLS

/*
 * EventdProtocolJson
 */
typedef struct _EventdProtocolJson EventdProtocolJson;
typedef struct _EventdProtocolJsonClass EventdProtocolJsonClass;
typedef struct _EventdProtocolJsonPrivate EventdProtocolJsonPrivate;

GType eventd_protocol_json_get_type(void) G_GNUC_CONST;

#define EVENTD_TYPE_PROTOCOL_JSON            (eventd_protocol_json_get_type())
#define EVENTD_PROTOCOL_JSON(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), EVENTD_TYPE_PROTOCOL_JSON, EventdProtocolJson))
#define EVENTD_IS_PROTOCOL_JSON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), EVENTD_TYPE_PROTOCOL_JSON))
#define EVENTD_PROTOCOL_JSON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), EVENTD_TYPE_PROTOCOL_JSON, EventdProtocolJsonClass))
#define EVENTD_IS_PROTOCOL_JSON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), EVENTD_TYPE_PROTOCOL_JSON))
#define EVENTD_PROTOCOL_JSON_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EVENTD_TYPE_PROTOCOL_JSON, EventdProtocolJsonClass))

struct _EventdProtocolJson
{
    GObject parent_object;

    /*< private >*/
    EventdProtocolJsonPrivate *priv;
};

struct _EventdProtocolJsonClass
{
    GObjectClass parent_class;
};

/*
 * Functions
 */

EventdProtocol *eventd_protocol_json_new(void);

G_END_DECLS

#endif /* __EVENTD_PROTOCOL_JSON_H__ */
