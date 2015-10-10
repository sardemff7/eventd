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

#include <uuid.h>

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
_eventd_protocol_json_malloc(gpointer user_data, gsize s)
{
    return g_malloc(s);
}

static gpointer
_eventd_protocol_json_realloc(gpointer user_data, gpointer data, gsize s)
{
    return g_realloc(data, s);
}

static void
_eventd_protocol_json_free(gpointer user_data, gpointer data)
{
    g_free(data);
}

yajl_alloc_funcs eventd_protocol_json_alloc_funcs = {
    .malloc = _eventd_protocol_json_malloc,
    .realloc = _eventd_protocol_json_realloc,
    .free = _eventd_protocol_json_free
};

#define EVENTD_PROTOCOL_JSON_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), EVENTD_TYPE_PROTOCOL_JSON, EventdProtocolJsonPrivate))

static void _eventd_protocol_json_parser_interface_init(EventdProtocolInterface *iface);

EVENTD_EXPORT GType eventd_protocol_json_get_type(void);
G_DEFINE_TYPE_WITH_CODE(EventdProtocolJson, eventd_protocol_json, G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE(EVENTD_TYPE_PROTOCOL, _eventd_protocol_json_parser_interface_init))


static void _eventd_protocol_json_finalize(GObject *object);

static void
eventd_protocol_json_class_init(EventdProtocolJsonClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(EventdProtocolJsonPrivate));

    eventd_protocol_json_parent_class = g_type_class_peek_parent(klass);

    object_class->finalize = _eventd_protocol_json_finalize;
}


static void
_eventd_protocol_json_parser_interface_init(EventdProtocolInterface *iface)
{
    iface->parse = eventd_protocol_json_parse;

    iface->generate_event = eventd_protocol_json_generate_event;
    iface->generate_answered = eventd_protocol_json_generate_answered;
    iface->generate_ended = eventd_protocol_json_generate_ended;

    iface->generate_passive = eventd_protocol_json_generate_passive;
    iface->generate_subscribe = eventd_protocol_json_generate_subscribe;
    iface->generate_bye = eventd_protocol_json_generate_bye;
}

/**
 * eventd_protocol_json_new:
 *
 * Returns: (transfer full) (type EventdProtocolJson): An #EventdProtocol for EvP
 */
EVENTD_EXPORT
EventdProtocol *
eventd_protocol_json_new(void)
{
    EventdProtocol *self;

    self = g_object_new(EVENTD_TYPE_PROTOCOL_JSON, NULL);

    return self;
}

static void
eventd_protocol_json_init(EventdProtocolJson *self)
{
    self->priv = EVENTD_PROTOCOL_JSON_GET_PRIVATE(self);

    self->priv->events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);
    uuid_clear(self->priv->message.uuid);
}

static void
_eventd_protocol_json_finalize(GObject *object)
{
    EventdProtocolJson *self = EVENTD_PROTOCOL_JSON(object);

    g_hash_table_unref(self->priv->events);

    eventd_protocol_json_parse_free(self);

    G_OBJECT_CLASS(eventd_protocol_json_parent_class)->finalize(object);
}


void
eventd_protocol_json_add_event(EventdProtocolJson *self, EventdEvent *event)
{
    if ( g_hash_table_contains(self->priv->events, eventd_event_get_uuid(event)) )
        return;
    g_hash_table_insert(self->priv->events, g_strdup(eventd_event_get_uuid(event)), g_object_ref(event));
}

void
eventd_protocol_json_remove_event(EventdProtocolJson *self, EventdEvent *event)
{
    g_hash_table_remove(self->priv->events, eventd_event_get_uuid(event));
}
