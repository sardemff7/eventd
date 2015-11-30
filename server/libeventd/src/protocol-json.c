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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <glib.h>
#include <glib-object.h>

#include <libeventd-event.h>
#include <libeventd-protocol.h>
#include <libeventd-event-private.h>
#include <libeventd-protocol-json.h>

#include "protocol-json-private.h"

/*
 * YAJL alloc functions
 */

static gpointer
_eventd_protocol_json_alloc_malloc(gpointer user_data, gsize s)
{
    return g_malloc(s);
}

static gpointer
_eventd_protocol_json_alloc_realloc(gpointer user_data, gpointer data, gsize s)
{
    return g_realloc(data, s);
}

static void
_eventd_protocol_json_alloc_free(gpointer user_data, gpointer data)
{
    g_free(data);
}

yajl_alloc_funcs eventd_protocol_json_alloc_funcs = {
    .malloc  = _eventd_protocol_json_alloc_malloc,
    .realloc = _eventd_protocol_json_alloc_realloc,
    .free    = _eventd_protocol_json_alloc_free
};

static void
_eventd_protocol_json_free(EventdProtocol *protocol)
{
    EventdProtocolJson *self = (EventdProtocolJson *) protocol;

    eventd_protocol_json_parse_free(self);

    g_free(self);
}

/**
 * eventd_protocol_json_new:
 *
 * Returns: (transfer full): An #EventdProtocol for JSON
 */
EVENTD_EXPORT
EventdProtocol *
eventd_protocol_json_new(const EventdProtocolCallbacks *callbacks, gpointer user_data, GDestroyNotify notify)
{
    EventdProtocol *protocol;

    protocol = eventd_protocol_new(sizeof(EventdProtocolJson), callbacks, user_data, notify);
    protocol->free = _eventd_protocol_json_free;

    protocol->parse = eventd_protocol_json_parse;

    protocol->generate_event = eventd_protocol_json_generate_event;
    protocol->generate_subscribe = eventd_protocol_json_generate_subscribe;
    protocol->generate_bye = eventd_protocol_json_generate_bye;

    return protocol;
}
