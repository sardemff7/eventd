/*
 * libeventd-protocol - Main eventd library - Protocol manipulation
 *
 * Copyright © 2011-2024 Morgane "Sardem FF7" Glidic
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

#include "config.h"

#include <string.h>

#include <glib.h>
#include <glib-object.h>

#include "libeventd-event.h"
#include "libeventd-protocol.h"
#include "libeventd-event-private.h"

#include "protocol-evp-private.h"

EVENTD_EXPORT GType eventd_protocol_get_type(void);
G_DEFINE_BOXED_TYPE(EventdProtocol, eventd_protocol, eventd_protocol_ref, eventd_protocol_unref)

static void
_eventd_protocol_evp_free(EventdProtocol *protocol)
{
    EventdProtocol *self = (EventdProtocol *) protocol;

    eventd_protocol_evp_parse_free(self);

    g_free(self);
}

/**
 * eventd_protocol_ref:
 * @protocol: an #EventdProtocol
 *
 * Increments the reference counter of @protocol.
 *
 * Returns: (transfer full): the #EventdProtocol
 */
EVENTD_EXPORT
EventdProtocol *
eventd_protocol_ref(EventdProtocol *self)
{
    g_return_val_if_fail(self != NULL, NULL);

    ++self->refcount;

    return self;
}

/**
 * eventd_protocol_unref:
 * @protocol: an #EventdProtocol
 *
 * Decrements the reference counter of @protocol.
 * If it reaches 0, free @protocol.
 */
EVENTD_EXPORT
void
eventd_protocol_unref(EventdProtocol *self)
{
    g_return_if_fail(self != NULL);

    if ( --self->refcount > 0 )
        return;

    if ( self->notify != NULL )
        self->notify(self->user_data);

    _eventd_protocol_evp_free(self);
}

/**
 * eventd_protocol_new:
 *
 * Returns: (transfer full): An #EventdProtocol
 */
EVENTD_EXPORT
EventdProtocol *
eventd_protocol_new(const EventdProtocolCallbacks *callbacks, gpointer user_data, GDestroyNotify notify)
{
    EventdProtocol *self;

    self = g_new0(EventdProtocol, 1);
    self->refcount = 1;

    self->callbacks = callbacks;
    self->user_data = user_data;
    self->notify = notify;

    return self;
}


/*
 * EventdProtocolParseError
 */

EVENTD_EXPORT
GQuark
eventd_protocol_parse_error_quark(void)
{
    return g_quark_from_static_string("eventd_protocol_parse_error-quark");
}
