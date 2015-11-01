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

EVENTD_EXPORT
guint eventd_protocol_signals[LAST_SIGNAL] = { 0 };

EVENTD_EXPORT GType eventd_protocol_get_type(void);
G_DEFINE_INTERFACE(EventdProtocol, eventd_protocol, 0);

static void
eventd_protocol_default_init(EventdProtocolInterface *iface)
{
    /**
     * EventdProtocol::event:
     * @protocol: the #EventdProtocol that parsed the message
     * @event: the #EventdEvent that was sent
     *
     * Emitted when parsed an EVENT message.
     */
    eventd_protocol_signals[SIGNAL_EVENT] =
        g_signal_new("event",
                     G_TYPE_FROM_INTERFACE(iface),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(EventdProtocolInterface, event),
                     NULL, NULL,
                     g_cclosure_marshal_generic,
                     G_TYPE_NONE, 1, EVENTD_TYPE_EVENT);

    /**
     * EventdProtocol::updated:
     * @protocol: the #EventdProtocol that parsed the message
     * @event: the #EventdEvent that was updated
     *
     * Emitted when parsed a UPDATED message.
     */
    eventd_protocol_signals[SIGNAL_UPDATED] =
        g_signal_new("updated",
                     G_TYPE_FROM_INTERFACE(iface),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(EventdProtocolInterface, updated),
                     NULL, NULL,
                     g_cclosure_marshal_generic,
                     G_TYPE_NONE, 1, EVENTD_TYPE_EVENT);

    /**
     * EventdProtocol::answered:
     * @protocol: the #EventdProtocol that parsed the message
     * @event: the #EventdEvent that was answered remotely
     * @answer: the answer
     *
     * Emitted when parsed a ANSWERED message.
     */
    eventd_protocol_signals[SIGNAL_ANSWERED] =
        g_signal_new("answered",
                     G_TYPE_FROM_INTERFACE(iface),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(EventdProtocolInterface, answered),
                     NULL, NULL,
                     g_cclosure_marshal_generic,
                     G_TYPE_NONE, 2, EVENTD_TYPE_EVENT, G_TYPE_STRING);

    /**
     * EventdProtocol::ended:
     * @protocol: the #EventdProtocol that parsed the message
     * @event: the #EventdEvent that ended remotely
     * @reason: the end reason
     *
     * Emitted when parsed a ENDED message.
     */
    eventd_protocol_signals[SIGNAL_ENDED] =
        g_signal_new("ended",
                     G_TYPE_FROM_INTERFACE(iface),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(EventdProtocolInterface, ended),
                     NULL, NULL,
                     g_cclosure_marshal_generic,
                     G_TYPE_NONE, 2, EVENTD_TYPE_EVENT, EVENTD_TYPE_EVENT_END_REASON);


    /**
     * EventdProtocol::passive:
     * @protocol: the #EventdProtocol that parsed the message
     *
     * Emitted when parsed a PASSIVE message.
     */
    eventd_protocol_signals[SIGNAL_PASSIVE] =
        g_signal_new("passive",
                     G_TYPE_FROM_INTERFACE(iface),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(EventdProtocolInterface, passive),
                     NULL, NULL,
                     g_cclosure_marshal_generic,
                     G_TYPE_NONE, 0);

    /**
     * EventdProtocol::subscribe:
     * @protocol: the #EventdProtocol that parsed the message
     * @categories: (element-type utf8 utf8) (nullable): the categories of events the client wants to subscribe to as a set (key == value)
     *
     * Emitted when parsed a SUBSCRIBE message.
     */
    eventd_protocol_signals[SIGNAL_SUBSCRIBE] =
        g_signal_new("subscribe",
                     G_TYPE_FROM_INTERFACE(iface),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(EventdProtocolInterface, subscribe),
                     NULL, NULL,
                     g_cclosure_marshal_generic,
                     G_TYPE_NONE, 1, G_TYPE_HASH_TABLE);

    /**
     * EventdProtocol::bye:
     * @protocol: the #EventdProtocol that parsed the message
     * @message: (nullable): the optional message
     *
     * Emitted when parsed a BYE message.
     */
    eventd_protocol_signals[SIGNAL_BYE] =
        g_signal_new("bye",
                     G_TYPE_FROM_INTERFACE(iface),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(EventdProtocolInterface, bye),
                     NULL, NULL,
                     g_cclosure_marshal_generic,
                     G_TYPE_NONE, 1, G_TYPE_STRING);
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
    g_return_val_if_fail(EVENTD_IS_PROTOCOL(self), FALSE);
    g_return_val_if_fail(buffer != NULL && *buffer != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    return EVENTD_PROTOCOL_GET_INTERFACE(self)->parse(self, buffer, error);
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
    g_return_val_if_fail(EVENTD_IS_PROTOCOL(self), NULL);
    g_return_val_if_fail(EVENTD_IS_EVENT(event), NULL);

    return EVENTD_PROTOCOL_GET_INTERFACE(self)->generate_event(self, event);
}

/**
 * eventd_protocol_generate_updated:
 * @protocol: an #EventdProtocol
 * @event: the #EventdEVent to generate a message for
 *
 * Generates an UPDATED message.
 *
 * Returns: (transfer full): the message
 */
EVENTD_EXPORT
gchar *
eventd_protocol_generate_updated(EventdProtocol *self, EventdEvent *event)
{
    g_return_val_if_fail(EVENTD_IS_PROTOCOL(self), NULL);
    g_return_val_if_fail(EVENTD_IS_EVENT(event), NULL);

    return EVENTD_PROTOCOL_GET_INTERFACE(self)->generate_updated(self, event);
}

/**
 * eventd_protocol_generate_answered:
 * @protocol: an #EventdProtocol
 * @event: the #EventdEVent to generate a message for
 * @answer: the answer to put in the message
 *
 * Generates an ANSWERED message.
 *
 * Returns: (transfer full): the message
 */
EVENTD_EXPORT
gchar *
eventd_protocol_generate_answered(EventdProtocol *self, EventdEvent *event, const gchar *answer)
{
    g_return_val_if_fail(EVENTD_IS_PROTOCOL(self), NULL);
    g_return_val_if_fail(EVENTD_IS_EVENT(event), NULL);
    g_return_val_if_fail(answer != NULL, NULL);

    return EVENTD_PROTOCOL_GET_INTERFACE(self)->generate_answered(self, event, answer);
}

/**
 * eventd_protocol_generate_ended:
 * @protocol: an #EventdProtocol
 * @event: the #EventdEVent to generate a message for
 * @reason: the end reason to put in the message
 *
 * Generates an ENDED message.
 *
 * Returns: (transfer full): the message
 */
EVENTD_EXPORT
gchar *
eventd_protocol_generate_ended(EventdProtocol *self, EventdEvent *event, EventdEventEndReason reason)
{
    g_return_val_if_fail(EVENTD_IS_PROTOCOL(self), NULL);
    g_return_val_if_fail(EVENTD_IS_EVENT(event), NULL);
    g_return_val_if_fail(eventd_event_end_reason_is_valid_value(reason), NULL);

    return EVENTD_PROTOCOL_GET_INTERFACE(self)->generate_ended(self, event, reason);
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
    g_return_val_if_fail(EVENTD_IS_PROTOCOL(self), NULL);

    return EVENTD_PROTOCOL_GET_INTERFACE(self)->generate_passive(self);
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
    g_return_val_if_fail(EVENTD_IS_PROTOCOL(self), NULL);
    g_return_val_if_fail(categories == NULL || g_hash_table_size(categories) > 0, NULL);

    return EVENTD_PROTOCOL_GET_INTERFACE(self)->generate_subscribe(self, categories);
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
    g_return_val_if_fail(EVENTD_IS_PROTOCOL(self), NULL);

    return EVENTD_PROTOCOL_GET_INTERFACE(self)->generate_bye(self, message);
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
