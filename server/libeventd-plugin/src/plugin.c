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

#define LIBEVENTD_PLUGIN_INTERFACE_ADD_CALLBACK_DEF(action, Action) EVENTD_EXPORT LIBEVENTD_PLUGIN_INTERFACE_ADD_CALLBACK(action, Action) { interface->action = callback; }


LIBEVENTD_PLUGIN_INTERFACE_ADD_CALLBACK_DEF(init, Init);
LIBEVENTD_PLUGIN_INTERFACE_ADD_CALLBACK_DEF(uninit, Simple);

LIBEVENTD_PLUGIN_INTERFACE_ADD_CALLBACK_DEF(get_option_group, GetOptionGroup);

LIBEVENTD_PLUGIN_INTERFACE_ADD_CALLBACK_DEF(start, Simple);
LIBEVENTD_PLUGIN_INTERFACE_ADD_CALLBACK_DEF(stop, Simple);

LIBEVENTD_PLUGIN_INTERFACE_ADD_CALLBACK_DEF(control_command, ControlCommand);

LIBEVENTD_PLUGIN_INTERFACE_ADD_CALLBACK_DEF(global_parse, GlobalParse);
LIBEVENTD_PLUGIN_INTERFACE_ADD_CALLBACK_DEF(event_parse, EventParse);
LIBEVENTD_PLUGIN_INTERFACE_ADD_CALLBACK_DEF(config_reset, Simple);

LIBEVENTD_PLUGIN_INTERFACE_ADD_CALLBACK_DEF(event_action, EventDispatch);
