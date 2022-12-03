/*
 * libeventd-plugin - Library to implement an eventd plugin
 *
 * Copyright © 2011-2021 Quentin "Sardem FF7" Glidic
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

#include "eventd-plugin.h"
#include "eventd-plugin-private.h"

/**
 * eventd_plugin_core_get_binds:
 * @context: an #EventdPluginCoreContext
 * @binds: (array zero-terminated=1) (element-type utf8) (nullable): list of strings
 *
 * Returns: (element-type GSocket) (transfer full): list of sockets for the binds
 */
EVENTD_EXPORT
GList *
eventd_plugin_core_get_binds(EventdPluginCoreContext *context, const gchar *namespace, const gchar * const *binds)
{
    return ((EventdCoreInterface *) context)->get_binds(context, namespace, binds);
}

/**
 * eventd_plugin_core_get_sockets:
 * @context: an #EventdPluginCoreContext
 * @binds: (array zero-terminated=1) (element-type utf8) (nullable): list of strings
 *
 * Returns: (element-type GSocket) (transfer full): list of sockets for the binds
 */
EVENTD_EXPORT
GList *
eventd_plugin_core_get_sockets(EventdPluginCoreContext *context, const gchar *namespace, GSocketAddress **binds)
{
    return ((EventdCoreInterface *) context)->get_sockets(context, namespace, binds);
}

/**
 * eventd_plugin_core_push_event:
 * @context: an #EventdPluginCoreContext
 * @event: an #EventdEvent
 *
 * Pushes an event into the queue.
 *
 * Returns: %TRUE if the event was pushed successfully.
 */
EVENTD_EXPORT
gboolean
eventd_plugin_core_push_event(EventdPluginCoreContext *context, EventdEvent *event)
{
    return ((EventdCoreInterface *) context)->push_event(context, event);
}
