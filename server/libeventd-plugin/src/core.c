/*
 * libeventd-plugin - Library to implement an eventd plugin
 *
 * Copyright © 2011-2012 Quentin "Sardem FF7" Glidic
 *
 * This file is part of eventd.
 *
 * eventd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * eventd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with eventd. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <glib.h>

#include <eventd-plugin.h>
#include <eventd-plugin-private.h>

/**
 * eventd_plugin_core_get_sockets:
 * @context: an #EventdPluginCoreContext
 * @interface: an #EventdPluginCoreInterface
 * @binds: (array zero-terminated=1) (element-type utf8) (allow-none): list of strings
 *
 * Returns: (element-type Gio.Socket) (transfer full): list of sockets for the binds
 */
EVENTD_EXPORT
GList *
eventd_plugin_core_get_sockets(EventdPluginCoreContext *context, EventdPluginCoreInterface *interface, const gchar * const *binds)
{
    return interface->get_sockets(context, binds);
}

EVENTD_EXPORT
gboolean
eventd_plugin_core_push_event(EventdPluginCoreContext *context, EventdPluginCoreInterface *interface, EventdEvent *event)
{
    return interface->push_event(context, event);

}
