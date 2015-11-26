/*
 * libeventd-event - Main eventd library - Event manipulation
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

#include <config.h>

#include <glib.h>
#include <glib-object.h>

#include <nkutils-uuid.h>

#include <libeventd-event.h>
#include <libeventd-event-private.h>

#include "event-private.h"

#define EVENTD_EVENT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), EVENTD_TYPE_EVENT, EventdEventPrivate))

EVENTD_EXPORT GType eventd_event_get_type(void);
G_DEFINE_TYPE(EventdEvent, eventd_event, G_TYPE_OBJECT)

/**
 * EventdEventEndReason:
 * @EVENTD_EVENT_END_REASON_NONE: No reason given.
 * @EVENTD_EVENT_END_REASON_DISCARD: The event is discarded, the server has no relevant configuration for it.
 * @EVENTD_EVENT_END_REASON_TIMEOUT: The event timed out.
 * @EVENTD_EVENT_END_REASON_USER_DISMISS: The user dismissed the event.
 * @EVENTD_EVENT_END_REASON_CLIENT_DISMISS: The source of the event dismissed the event.
 * @EVENTD_EVENT_END_REASON_TEST: Test reason. To be used in tests.
 * @EVENTD_EVENT_END_REASON_UNKNOWN: Unknown reason. When the reason comes from a remote server, the value may have been introduced in a new version.
 * @EVENTD_EVENT_END_REASON_RESERVED: Internal use only.
 */

EVENTD_EXPORT
GType
eventd_event_end_reason_get_type(void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if ( g_once_init_enter(&g_define_type_id__volatile))
    {
        static const GEnumValue values[] = {
            { EVENTD_EVENT_END_REASON_NONE,           "EVENTD_EVENT_END_REASON_NONE",           "none" },
            { EVENTD_EVENT_END_REASON_DISCARD,        "EVENTD_EVENT_END_REASON_DISCARD",        "discard" },
            { EVENTD_EVENT_END_REASON_TIMEOUT,        "EVENTD_EVENT_END_REASON_TIMEOUT",        "timeout" },
            { EVENTD_EVENT_END_REASON_USER_DISMISS,   "EVENTD_EVENT_END_REASON_USER_DISMISS",   "user-dismiss" },
            { EVENTD_EVENT_END_REASON_CLIENT_DISMISS, "EVENTD_EVENT_END_REASON_CLIENT_DISMISS", "client-dismiss" },
            { EVENTD_EVENT_END_REASON_TEST,           "EVENTD_EVENT_END_REASON_TEST",           "test" },
            { EVENTD_EVENT_END_REASON_UNKNOWN,        "EVENTD_EVENT_END_REASON_UNKNOWN",        "unknown" },
            { EVENTD_EVENT_END_REASON_RESERVED,       "EVENTD_EVENT_END_REASON_RESERVED",       "reserved" },
            { 0, NULL, NULL }
        };
        GType g_define_type_id = g_enum_register_static(g_intern_static_string("EventdEventEndReason"), values);
        g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}

/**
 * eventd_event_end_reason_is_valid_value:
 * @reason: an #EventdEventEndReason
 *
 * Checks if @reason is a valid #EventdEventEndReason value.
 *
 * Returns: %TRUE if @reason is a valid #EventdEventEndReason value.
 */
EVENTD_EXPORT
gboolean
eventd_event_end_reason_is_valid_value(EventdEventEndReason reason)
{
    GEnumClass *enum_class;
    GEnumValue *enum_value;

    enum_class = g_type_class_ref(EVENTD_TYPE_EVENT_END_REASON);
    enum_value = g_enum_get_value(enum_class, reason);
    g_type_class_unref(enum_class);

    return ( enum_value != NULL );
}

/**
 * eventd_event_end_reason_get_value_name:
 * @reason: an #EventdEventEndReason
 *
 * Looks up the name of @reason.
 *
 * Returns: (transfer none): the name of @reason
 */
EVENTD_EXPORT
const gchar *
eventd_event_end_reason_get_value_name(EventdEventEndReason reason)
{
    g_return_val_if_fail(eventd_event_end_reason_is_valid_value(reason), NULL);

    GEnumClass *enum_class;
    GEnumValue *enum_value;
    const gchar *name;

    enum_class = g_type_class_ref(EVENTD_TYPE_EVENT_END_REASON);
    enum_value = g_enum_get_value(enum_class, reason);
    name = enum_value->value_name;
    g_type_class_unref(enum_class);

    /*
     * It is safe to return this pointer here because we
     * set it in eventd_event_end_reason_get_type and it
     * is a compile-time const string
     */
    return name;
}

/**
 * eventd_event_end_reason_get_value_nick:
 * @reason: an #EventdEventEndReason
 *
 * CLooks up the nickname of @reason.
 *
 * Returns: (transfer none): the nickname of @reason
 */
EVENTD_EXPORT
const gchar *
eventd_event_end_reason_get_value_nick(EventdEventEndReason reason)
{
    g_return_val_if_fail(eventd_event_end_reason_is_valid_value(reason), NULL);

    GEnumClass *enum_class;
    GEnumValue *enum_value;
    const gchar *nick;

    enum_class = g_type_class_ref(EVENTD_TYPE_EVENT_END_REASON);
    enum_value = g_enum_get_value(enum_class, reason);
    nick = enum_value->value_nick;
    g_type_class_unref(enum_class);

    /*
     * It is safe to return this pointer here because we
     * set it in eventd_event_end_reason_get_type and it
     * is a compile-time const string
     */
    return nick;
}


enum {
    SIGNAL_ANSWERED,
    SIGNAL_ENDED,
    LAST_SIGNAL
};

static guint _eventd_event_signals[LAST_SIGNAL] = { 0 };


static void _eventd_event_finalize(GObject *object);

static void
eventd_event_class_init(EventdEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(EventdEventPrivate));

    eventd_event_parent_class = g_type_class_peek_parent(klass);

    object_class->finalize = _eventd_event_finalize;

    /**
     * EventdEvent::answered:
     * @event: the #EventdEvent that was answered
     * @answer: (transfer none): the answer given
     *
     * Emitted when an event is answered.
     */
    _eventd_event_signals[SIGNAL_ANSWERED] =
        g_signal_new("answered",
                     G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(EventdEventClass, answered),
                     NULL, NULL,
                     g_cclosure_marshal_generic,
                     G_TYPE_NONE, 1, G_TYPE_STRING);

    /**
     * EventdEvent::ended:
     * @event: the #EventdEvent that ended
     * @reason: an #EventdEventEndReason for the end
     *
     * Emitted when an event is complete.
     */
    _eventd_event_signals[SIGNAL_ENDED] =
        g_signal_new("ended",
                     G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(EventdEventClass, ended),
                     NULL, NULL,
                     g_cclosure_marshal_generic,
                     G_TYPE_NONE, 1, EVENTD_TYPE_EVENT_END_REASON);
}

/**
 * eventd_event_new:
 * @category: the category of the event
 * @name: the name of the event
 *
 * Creates a new #EventdEvent.
 *
 * Returns: (transfer full): a new #EventdEvent
 */
EVENTD_EXPORT
EventdEvent *
eventd_event_new(const gchar *category, const gchar *name)
{
    NkUuid uuid;
    EventdEvent *self;

    nk_uuid_generate(&uuid);
    self = eventd_event_new_for_uuid(uuid, category, name);

    return self;
}

static void
eventd_event_init(EventdEvent *self)
{
    self->priv = EVENTD_EVENT_GET_PRIVATE(self);

    self->priv->timeout = -1;
}

static void
_eventd_event_finalize(GObject *object)
{
    EventdEvent *self = EVENTD_EVENT(object);

    if ( self->priv->data != NULL )
        g_hash_table_unref(self->priv->data);
    g_free(self->priv->name);

    G_OBJECT_CLASS(eventd_event_parent_class)->finalize(object);
}

/**
 * eventd_event_answer:
 * @event: an #EventdEvent
 * @answer: the answer to the event
 *
 * Responds to the event with the given answer.
 */
EVENTD_EXPORT
void
eventd_event_answer(EventdEvent *self, const gchar *answer)
{
    g_return_if_fail(EVENTD_IS_EVENT(self));
    GList *answer_;
    answer_ = g_list_find_custom(self->priv->answers, answer, (GCompareFunc)g_strcmp0);
    g_return_if_fail(answer_ != NULL);

    g_signal_emit(self, _eventd_event_signals[SIGNAL_ANSWERED], 0, answer_->data);
}

/**
 * eventd_event_end:
 * @event: an #EventdEvent
 * @reason: the reason for ending the event
 *
 * Terminates an event.
 */
EVENTD_EXPORT
void
eventd_event_end(EventdEvent *self, EventdEventEndReason reason)
{
    g_return_if_fail(EVENTD_IS_EVENT(self));
    g_return_if_fail(eventd_event_end_reason_is_valid_value(reason));

    g_signal_emit(self, _eventd_event_signals[SIGNAL_ENDED], 0, reason);
}

/**
 * eventd_event_add_data:
 * @event: an #EventdEvent
 * @name: (transfer full): a name for the data
 * @content: (transfer full): the data to add
 *
 * Attaches named data to the event.
 */
EVENTD_EXPORT
void
eventd_event_add_data(EventdEvent *self, gchar *name, gchar *content)
{
    g_return_if_fail(EVENTD_IS_EVENT(self));
    g_return_if_fail(name != NULL);
    g_return_if_fail(content != NULL);

    if ( self->priv->data == NULL )
        self->priv->data = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(self->priv->data, name, content);
}

/**
 * eventd_event_add_answer:
 * @event: an #EventdEvent
 * @name: an answer for the event
 *
 * Adds a valid answer value for the event.
 */
EVENTD_EXPORT
void
eventd_event_add_answer(EventdEvent *self, const gchar *name)
{
    g_return_if_fail(EVENTD_IS_EVENT(self));
    g_return_if_fail(name != NULL);

    self->priv->answers = g_list_prepend(self->priv->answers, g_strdup(name));
}

/**
 * eventd_event_add_answer_data:
 * @event: an #EventdEvent
 * @name: (transfer full): a name for the data
 * @content: (transfer full): the data to add
 *
 * Attaches named data to the answer.
 */
EVENTD_EXPORT
void
eventd_event_add_answer_data(EventdEvent *self, gchar *name, gchar *content)
{
    g_return_if_fail(EVENTD_IS_EVENT(self));
    g_return_if_fail(name != NULL);
    g_return_if_fail(content != NULL);

    if ( self->priv->answer_data == NULL )
        self->priv->answer_data = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(self->priv->answer_data, name, content);
}

/**
 * eventd_event_set_timeout:
 * @event: an #EventdEvent
 * @timeout: the event timeout (in milliseconds)
 *
 * Sets how long the event before be active before it timesout.
 */
EVENTD_EXPORT
void
eventd_event_set_timeout(EventdEvent *self, gint64 timeout)
{
    g_return_if_fail(EVENTD_IS_EVENT(self));
    g_return_if_fail(timeout >= -1);

    self->priv->timeout = timeout;
}


/**
 * eventd_event_get_category:
 * @event: an #EventdEvent
 *
 * Retrieves the category for the event.
 *
 * Returns: (transfer none): the category of the event
 */
EVENTD_EXPORT
const gchar *
eventd_event_get_category(const EventdEvent *self)
{
    g_return_val_if_fail(EVENTD_IS_EVENT(self), NULL);

    return self->priv->category;
}

/**
 * eventd_event_get_name:
 * @event: an #EventdEvent
 *
 * Retrieves the name for the event.
 *
 * Returns: (transfer none): the name of the event
 */
EVENTD_EXPORT
const gchar *
eventd_event_get_name(const EventdEvent *self)
{
    g_return_val_if_fail(EVENTD_IS_EVENT(self), NULL);

    return self->priv->name;
}

/**
 * eventd_event_get_timeout:
 * @event: an #EventdEvent
 *
 * Retrieves the timeout for the event.
 *
 * Returns: the timeout of the event
 */
EVENTD_EXPORT
gint64
eventd_event_get_timeout(const EventdEvent *self)
{
    g_return_val_if_fail(EVENTD_IS_EVENT(self), G_MININT64);

    return self->priv->timeout;
}

/**
 * eventd_event_has_data:
 * @event: an #EventdEvent
 * @name: a name of the data
 *
 * Retrieves whether the event has data with a given name.
 *
 * Returns: %TRUE if the event has data named @name
 */
EVENTD_EXPORT
gboolean
eventd_event_has_data(const EventdEvent *self, const gchar *name)
{
    g_return_val_if_fail(EVENTD_IS_EVENT(self), FALSE);
    g_return_val_if_fail(name != NULL, FALSE);

    if ( self->priv->data == NULL )
        return FALSE;

    return g_hash_table_contains(self->priv->data, name);
}

/**
 * eventd_event_get_data:
 * @event: an #EventdEvent
 * @name: a name of the data
 *
 * Retrieves the event data with a given name.
 *
 * Returns: (transfer none): the data in the event
 */
EVENTD_EXPORT
const gchar *
eventd_event_get_data(const EventdEvent *self, const gchar *name)
{
    g_return_val_if_fail(EVENTD_IS_EVENT(self), NULL);
    g_return_val_if_fail(name != NULL, NULL);

    if ( self->priv->data == NULL )
        return NULL;

    return g_hash_table_lookup(self->priv->data, name);
}

/**
 * eventd_event_get_all_data:
 * @event: an #EventdEvent
 *
 * Retrieves the data table from the event.
 *
 * Returns: (nullable) (transfer container) (element-type utf8 utf8): the data table
 */
EVENTD_EXPORT
GHashTable *
eventd_event_get_all_data(const EventdEvent *self)
{
    g_return_val_if_fail(EVENTD_IS_EVENT(self), NULL);

    if ( self->priv->data == NULL )
        return NULL;

    return g_hash_table_ref(self->priv->data);
}

/**
 * eventd_event_get_answer_data:
 * @event: an #EventdEvent
 * @name: a name of the answer data
 *
 * Retrieves the answer data with a given name.
 *
 * Returns: (transfer none): the data in the event for the answer
 */
EVENTD_EXPORT
const gchar *
eventd_event_get_answer_data(const EventdEvent *self, const gchar *name)
{
    g_return_val_if_fail(EVENTD_IS_EVENT(self), NULL);
    g_return_val_if_fail(name != NULL, NULL);

    if ( self->priv->answer_data == NULL )
        return NULL;

    return g_hash_table_lookup(self->priv->answer_data, name);
}

/**
 * eventd_event_get_all_answer_data:
 * @event: an #EventdEvent
 *
 * Retrieves the answer data table from the event.
 *
 * Returns: (nullable) (transfer container) (element-type utf8 utf8): the answer data table
 */
EVENTD_EXPORT
GHashTable *
eventd_event_get_all_answer_data(const EventdEvent *self)
{
    g_return_val_if_fail(EVENTD_IS_EVENT(self), NULL);

    if ( self->priv->answer_data == NULL )
        return NULL;

    return g_hash_table_ref(self->priv->answer_data);
}
