/*
 * libeventd-event - Main eventd library - Event manipulation
 *
 * Copyright © 2011-2018 Quentin "Sardem FF7" Glidic
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

#include <glib.h>
#include <glib-object.h>

#include <nkutils-uuid.h>

#include "libeventd-event.h"
#include "libeventd-event-private.h"

#include "event-private.h"

EVENTD_EXPORT GType eventd_event_get_type(void);
G_DEFINE_BOXED_TYPE(EventdEvent, eventd_event, eventd_event_ref, eventd_event_unref)


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
_eventd_event_free(EventdEvent *self)
{
    if ( self->data != NULL )
        g_hash_table_unref(self->data);
    g_free(self->name);
}

/**
 * eventd_event_ref:
 * @event: an #EventdEvent
 *
 * Increments the reference counter of @event.
 *
 * Returns: (transfer full): the #EventdEvent
 */
EVENTD_EXPORT
EventdEvent *
eventd_event_ref(EventdEvent *self)
{
    g_return_val_if_fail(self != NULL, NULL);

    ++self->refcount;

    return self;
}

/**
 * eventd_event_unref:
 * @event: (nullable): an #EventdEvent
 *
 * Decrements the reference counter of @event.
 * If it reaches 0, free @event.
 *
 * Can safely be called on %NULL.
 */
EVENTD_EXPORT
void
eventd_event_unref(EventdEvent *self)
{
    if ( self == NULL )
        return;

    if ( --self->refcount < 1 )
        _eventd_event_free(self);
}

/**
 * eventd_event_add_data:
 * @event: an #EventdEvent
 * @name: (transfer full): a name for the data
 * @content: the data as a #GVariant
 *
 * Attaches named data to the event.
 *
 * If @content is a floating reference (see g_variant_ref_sink()),
 * the @event takes ownership of @content.
 */
EVENTD_EXPORT
void
eventd_event_add_data(EventdEvent *self, gchar *name, GVariant *content)
{
    g_return_if_fail(self != NULL);
    g_return_if_fail(name != NULL);
    g_return_if_fail(content != NULL);

    if ( self->data == NULL )
        self->data = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_variant_unref);
    g_hash_table_insert(self->data, name, g_variant_ref_sink(content));
}

/**
 * eventd_event_add_data_string:
 * @event: an #EventdEvent
 * @name: (transfer full): a name for the data
 * @content: (transfer full): the data to add
 *
 * Attaches named data to the event.
 */
EVENTD_EXPORT
void
eventd_event_add_data_string(EventdEvent *self, gchar *name, gchar *content)
{
    g_return_if_fail(self != NULL);
    g_return_if_fail(name != NULL);
    g_return_if_fail(content != NULL);

    eventd_event_add_data(self, name, g_variant_new_take_string(content));
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
    g_return_val_if_fail(self != NULL, NULL);

    return self->uuid.string;
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
    g_return_val_if_fail(self != NULL, NULL);

    return self->category;
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
    g_return_val_if_fail(self != NULL, NULL);

    return self->name;
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
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(name != NULL, FALSE);

    if ( self->data == NULL )
        return FALSE;

    return g_hash_table_contains(self->data, name);
}

/**
 * eventd_event_get_data:
 * @event: an #EventdEvent
 * @name: a name of the data
 *
 * Retrieves the event data with a given name.
 *
 * Returns: (transfer none): the data in the event, as a #GVariant
 */
EVENTD_EXPORT
GVariant *
eventd_event_get_data(const EventdEvent *self, const gchar *name)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_return_val_if_fail(name != NULL, NULL);

    if ( self->data == NULL )
        return NULL;

    return g_hash_table_lookup(self->data, name);
}

/**
 * eventd_event_get_data_string:
 * @event: an #EventdEvent
 * @name: a name of the data
 *
 * Retrieves the event data with a given name as a string.
 *
 * Returns: (transfer none): the data in the event
 */
EVENTD_EXPORT
const gchar *
eventd_event_get_data_string(const EventdEvent *self, const gchar *name)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_return_val_if_fail(name != NULL, NULL);

    GVariant *data;
    data = eventd_event_get_data(self, name);
    if ( ( data == NULL ) || ( ! g_variant_is_of_type(data, G_VARIANT_TYPE_STRING) ) )
        return NULL;

    return g_variant_get_string(data, NULL);
}

/**
 * eventd_event_get_all_data:
 * @event: an #EventdEvent
 *
 * Retrieves the data table from the event.
 *
 * Returns: (nullable) (transfer container) (element-type utf8 GVariant): the data table
 */
EVENTD_EXPORT
GHashTable *
eventd_event_get_all_data(const EventdEvent *self)
{
    g_return_val_if_fail(self != NULL, NULL);

    if ( self->data == NULL )
        return NULL;

    return g_hash_table_ref(self->data);
}
