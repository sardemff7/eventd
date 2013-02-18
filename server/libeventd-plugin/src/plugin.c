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

EVENTD_EXPORT void libeventd_plugin_interface_add_init_callback(EventdPluginInterface *interface, EventdPluginInitFunc callback) { interface->init = callback; }
EVENTD_EXPORT void libeventd_plugin_interface_add_uninit_callback(EventdPluginInterface *interface, EventdPluginSimpleFunc callback) { interface->uninit = callback; }

EVENTD_EXPORT void libeventd_plugin_interface_add_get_option_group_callback(EventdPluginInterface *interface, EventdPluginGetOptionGroupFunc callback) { interface->get_option_group = callback; }

EVENTD_EXPORT void libeventd_plugin_interface_add_start_callback(EventdPluginInterface *interface, EventdPluginSimpleFunc callback) { interface->start = callback; }
EVENTD_EXPORT void libeventd_plugin_interface_add_stop_callback(EventdPluginInterface *interface, EventdPluginSimpleFunc callback) { interface->stop = callback; }

EVENTD_EXPORT void libeventd_plugin_interface_add_control_command_callback(EventdPluginInterface *interface, EventdPluginControlCommandFunc callback) { interface->control_command = callback; }

EVENTD_EXPORT void libeventd_plugin_interface_add_global_parse_callback(EventdPluginInterface *interface, EventdPluginGlobalParseFunc callback) { interface->global_parse = callback; }
EVENTD_EXPORT void libeventd_plugin_interface_add_event_parse_callback(EventdPluginInterface *interface, EventdPluginEventParseFunc callback) { interface->event_parse = callback; }
EVENTD_EXPORT void libeventd_plugin_interface_add_config_reset_callback(EventdPluginInterface *interface, EventdPluginSimpleFunc callback) { interface->config_reset = callback; }

EVENTD_EXPORT void libeventd_plugin_interface_add_event_action_callback(EventdPluginInterface *interface, EventdPluginEventDispatchFunc callback) { interface->event_action = callback; }
