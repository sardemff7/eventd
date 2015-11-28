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

static void _eventd_event_finalize(GObject *object);

static void
eventd_event_class_init(EventdEventClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(EventdEventPrivate));

    eventd_event_parent_class = g_type_class_peek_parent(klass);

    object_class->finalize = _eventd_event_finalize;
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
 * eventd_event_get_uuid:
 * @event: an #EventdEvent
 *
 * Retrieves the UUID (as a string) for the event.
 *
 * Returns: (transfer none): the UUID of the event
 */
EVENTD_EXPORT
const gchar *
eventd_event_get_uuid(EventdEvent *self)
{
    g_return_val_if_fail(EVENTD_IS_EVENT(self), NULL);

    return self->priv->uuid.string;
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
