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

#include "protocol-evp-private.h"

#define EVENTD_PROTOCOL_EVP_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), EVENTD_TYPE_PROTOCOL_EVP, EventdProtocolEvpPrivate))

static void _eventd_protocol_evp_parser_interface_init(EventdProtocolInterface *iface);

EVENTD_EXPORT GType eventd_protocol_evp_get_type(void);
G_DEFINE_TYPE_WITH_CODE(EventdProtocolEvp, eventd_protocol_evp, G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE(EVENTD_TYPE_PROTOCOL, _eventd_protocol_evp_parser_interface_init))


static void _eventd_protocol_evp_finalize(GObject *object);

static void
eventd_protocol_evp_class_init(EventdProtocolEvpClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(EventdProtocolEvpPrivate));

    eventd_protocol_evp_parent_class = g_type_class_peek_parent(klass);

    object_class->finalize = _eventd_protocol_evp_finalize;
}


static void
_eventd_protocol_evp_add_event(EventdProtocol *protocol, EventdEvent *event)
{
    g_return_if_fail(EVENTD_IS_PROTOCOL_EVP(protocol));
    EventdProtocolEvp *self = EVENTD_PROTOCOL_EVP(protocol);

    eventd_protocol_evp_add_event(self, event);
}

static void
_eventd_protocol_evp_remove_event(EventdProtocol *protocol, EventdEvent *event)
{
    g_return_if_fail(EVENTD_IS_PROTOCOL_EVP(protocol));
    EventdProtocolEvp *self = EVENTD_PROTOCOL_EVP(protocol);

    eventd_protocol_evp_remove_event(self, event);
}

static void
_eventd_protocol_evp_parser_interface_init(EventdProtocolInterface *iface)
{
    iface->add_event = _eventd_protocol_evp_add_event;
    iface->remove_event = _eventd_protocol_evp_remove_event;

    iface->parse = eventd_protocol_evp_parse;

    iface->generate_event = eventd_protocol_evp_generate_event;
    iface->generate_ended = eventd_protocol_evp_generate_ended;

    iface->generate_passive = eventd_protocol_evp_generate_passive;
    iface->generate_subscribe = eventd_protocol_evp_generate_subscribe;
    iface->generate_bye = eventd_protocol_evp_generate_bye;
}

/**
 * eventd_protocol_evp_new:
 *
 * Returns: (transfer full) (type EventdProtocolEvp): An #EventdProtocol for EvP
 */
EVENTD_EXPORT
EventdProtocol *
eventd_protocol_evp_new(void)
{
    EventdProtocol *self;

    self = g_object_new(EVENTD_TYPE_PROTOCOL_EVP, NULL);

    return self;
}

static void
eventd_protocol_evp_init(EventdProtocolEvp *self)
{
    self->priv = EVENTD_PROTOCOL_EVP_GET_PRIVATE(self);

    self->priv->events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);
}

static void
_eventd_protocol_evp_finalize(GObject *object)
{
    EventdProtocolEvp *self = EVENTD_PROTOCOL_EVP(object);

    g_hash_table_unref(self->priv->events);

    eventd_protocol_evp_parse_free(self);

    G_OBJECT_CLASS(eventd_protocol_evp_parent_class)->finalize(object);
}


void
eventd_protocol_evp_add_event(EventdProtocolEvp *self, EventdEvent *event)
{
    if ( g_hash_table_contains(self->priv->events, eventd_event_get_uuid(event)) )
        return;
    g_hash_table_insert(self->priv->events, g_strdup(eventd_event_get_uuid(event)), g_object_ref(event));
}

void
eventd_protocol_evp_remove_event(EventdProtocolEvp *self, EventdEvent *event)
{
    g_hash_table_remove(self->priv->events, eventd_event_get_uuid(event));
}
