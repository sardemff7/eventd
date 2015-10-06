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

#ifndef __EVENTD_PROTOCOL_PRIVATE_H__
#define __EVENTD_PROTOCOL_PRIVATE_H__

enum {
    SIGNAL_EVENT,
    SIGNAL_ANSWERED,
    SIGNAL_ENDED,
    SIGNAL_PASSIVE,
    SIGNAL_SUBSCRIBE,
    SIGNAL_BYE,
    LAST_SIGNAL
};

guint _eventd_protocol_signals[LAST_SIGNAL];

#endif /* __EVENTD_PROTOCOL_PRIVATE_H__ */
