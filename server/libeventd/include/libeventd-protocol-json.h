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
 * Functions
 */

EventdProtocol *eventd_protocol_json_new(const EventdProtocolCallbacks *callbacks, gpointer user_data, GDestroyNotify notify);

G_END_DECLS

#endif /* __EVENTD_PROTOCOL_JSON_H__ */
