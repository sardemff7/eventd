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

#include <glib.h>
#include <glib-object.h>

#include <libeventd-protocol.h>

#include "protocol-private.h"

EVENTD_EXPORT GType eventd_protocol_get_type(void);
G_DEFINE_BOXED_TYPE(EventdProtocol, eventd_protocol, eventd_protocol_ref, eventd_protocol_unref);

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

    self->free(self);
}

/**
 * eventd_protocol_parse:
 * @protocol: an #EventdProtocol
 * @buffer: a pointer to a buffer
 * @error: (out) (optional): return location for error or %NULL to ignore
 *
 * Parses @buffer for messages.
 *
 * Returns: %FALSE if there was an error, %TRUE otherwise
 */
EVENTD_EXPORT
gboolean
eventd_protocol_parse(EventdProtocol *self, gchar **buffer, GError **error)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(buffer != NULL && *buffer != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    return self->parse(self, buffer, error);
}

/**
 * eventd_protocol_generate_event:
 * @protocol: an #EventdProtocol
 * @event: the #EventdEVent to generate a message for
 *
 * Generates an EVENT message.
 *
 * Returns: (transfer full): the message
 */
EVENTD_EXPORT
gchar *
eventd_protocol_generate_event(EventdProtocol *self, EventdEvent *event)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_return_val_if_fail(EVENTD_IS_EVENT(event), NULL);

    return self->generate_event(self, event);
}

/**
 * eventd_protocol_generate_passive:
 * @protocol: an #EventdProtocol
 *
 * Generates a PASSIVE message.
 *
 * Returns: (transfer full): the message
 */
EVENTD_EXPORT
gchar *
eventd_protocol_generate_passive(EventdProtocol *self)
{
    g_return_val_if_fail(self != NULL, NULL);

    return self->generate_passive(self);
}

/**
 * eventd_protocol_generate_subscribe:
 * @protocol: an #EventdProtocol
 * @categories: (element-type utf8 utf8) (nullable): the categories of events you want to subscribe to as a set (key == value)
 *
 * Generates a SUBSCRIBE message.
 *
 * Returns: (transfer full): the message
 */
EVENTD_EXPORT
gchar *
eventd_protocol_generate_subscribe(EventdProtocol *self, GHashTable *categories)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_return_val_if_fail(categories == NULL || g_hash_table_size(categories) > 0, NULL);

    return self->generate_subscribe(self, categories);
}

/**
 * eventd_protocol_generate_bye:
 * @protocol: an #EventdProtocol
 * @message: (nullable): an optional message to send
 *
 * Generates a BYE message.
 *
 * Returns: (transfer full): the message
 */
EVENTD_EXPORT
gchar *
eventd_protocol_generate_bye(EventdProtocol *self, const gchar *message)
{
    g_return_val_if_fail(self != NULL, NULL);

    return self->generate_bye(self, message);
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
