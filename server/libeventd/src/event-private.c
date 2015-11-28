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


EventdEvent *
eventd_event_new_for_uuid(NkUuid uuid, const gchar *category, const gchar *name)
{
    EventdEvent *self;

    self = g_object_new(EVENTD_TYPE_EVENT, NULL);

    self->priv->uuid = uuid;
    self->priv->category = g_strdup(category);
    self->priv->name = g_strdup(name);

    return self;
}

EventdEvent *
eventd_event_new_for_uuid_string(const gchar *uuid_string, const gchar *category, const gchar *name)
{
    g_return_val_if_fail(uuid_string != NULL, NULL);
    g_return_val_if_fail(category != NULL, NULL);
    g_return_val_if_fail(name != NULL, NULL);

    NkUuid uuid;
    if ( ! nk_uuid_parse(&uuid, uuid_string) )
        return NULL;

    return eventd_event_new_for_uuid(uuid, category, name);
}

void
eventd_event_set_all_data(EventdEvent *self, GHashTable *data)
{
    g_return_if_fail(EVENTD_IS_EVENT(self));

    if ( self->priv->data != NULL )
        g_hash_table_unref(self->priv->data);
    self->priv->data = data;
}
