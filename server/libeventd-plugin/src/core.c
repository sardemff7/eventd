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
#include <eventd-plugin-interfaces.h>

EVENTD_EXPORT
GList *
libeventd_core_get_sockets(EventdCoreContext *context, EventdCoreInterface *interface, const gchar * const *binds)
{
    return interface->get_sockets(context, binds);
}

EVENTD_EXPORT
const gchar *
libeventd_core_get_event_config_id(EventdCoreContext *context, EventdCoreInterface *interface, EventdEvent *event)
{
    return interface->get_event_config_id(context, event);
}

EVENTD_EXPORT
void
libeventd_core_push_event(EventdCoreContext *context, EventdCoreInterface *interface, const gchar *config_id, EventdEvent *event)
{
    interface->push_event(context, config_id, event);

}
