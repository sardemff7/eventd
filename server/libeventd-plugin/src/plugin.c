/*
 * libeventd-plugin - Library to implement an eventd plugin
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

#include <eventd-plugin.h>
#include <eventd-plugin-private.h>

/**
 * SECTION: Plugins
 *
 * Eventd plugins are used for distributing events over various channels. A
 * plugin should have a symbol named eventd_plugin_id which is a name for the
 * plugin. In addition, an eventd_plugin_get_interface function matching
 * #EventdPluginGetInterfaceFunc is called to initialize the plugin.
 */

/**
 * EventdPluginCoreContext:
 *
 * An opaque pointer eventd uses to keep track of plugins.
 */

/**
 * EventdPluginCoreInterface:
 *
 * An opaque pointer eventd uses to keep track of a plugin.
 */

/**
 * EventdPluginContext:
 *
 * A plugin-defined structure which is used in all plugin interface functions.
 */

/**
 * EventdPluginInterface:
 *
 * An opaque pointer for holding callback functions.
 */

/**
 * EventdPluginSimpleFunc:
 * @context: the plugin-specific context
 */

/**
 * EventdPluginInitFunc:
 * @core: the context to use for the core functions
 * @interface: the plugin interface
 *
 * Returns: (transfer full): a plugin-specific context object
 */

/**
 * eventd_plugin_interface_add_init_callback:
 * @interface: an #EventdPluginInterface
 * @callback: (scope async): a function to call when the plugin is loaded
 *
 * The callback should return a context which is passed to all other functions.
 */
EVENTD_EXPORT void eventd_plugin_interface_add_init_callback(EventdPluginInterface *interface, EventdPluginInitFunc callback) { interface->init = callback; }
/**
 * eventd_plugin_interface_add_uninit_callback:
 * @interface: an #EventdPluginInterface
 * @callback: (scope async): a function to call when the plugin is unloaded
 *
 * The callback should destroy the context returned from
 * eventd_plugin_interface_add_init_callback().
 */
EVENTD_EXPORT void eventd_plugin_interface_add_uninit_callback(EventdPluginInterface *interface, EventdPluginSimpleFunc callback) { interface->uninit = callback; }

/**
 * EventdPluginGetOptionGroupFunc:
 * @context: the plugin-specific context
 *
 * Creates a #GOptionGroup for argument parsing. The name of the group should
 * match the plugin name.
 *
 * Returns: (transfer full): the #GOptionGroup for the plugin's subcommand
 */

/**
 * eventd_plugin_interface_add_get_option_group_callback:
 * @interface: an #EventdPluginInterface
 * @callback: (scope async): a function to call before parsing arguments
 *
 * The callback may add #GOptionGroup items for argument parsing.
 */
EVENTD_EXPORT void eventd_plugin_interface_add_get_option_group_callback(EventdPluginInterface *interface, EventdPluginGetOptionGroupFunc callback) { interface->get_option_group = callback; }

/**
 * eventd_plugin_interface_add_start_callback:
 * @interface: an #EventdPluginInterface
 * @callback: (scope async): a function to call after configuration is parsed
 *
 * This callback should set up any plugin-wide requirements (e.g., sockets,
 * files, databases, etc.) specified by the configuration. This is also called
 * after the configuration is reloaded.
 */
EVENTD_EXPORT void eventd_plugin_interface_add_start_callback(EventdPluginInterface *interface, EventdPluginSimpleFunc callback) { interface->start = callback; }
/**
 * eventd_plugin_interface_add_stop_callback:
 * @interface: an #EventdPluginInterface
 * @callback: (scope async): a function to call before shutdown
 *
 * This callback should tear down any plugin-wide resources. It is also called
 * before the configuration is reloaded.
 */
EVENTD_EXPORT void eventd_plugin_interface_add_stop_callback(EventdPluginInterface *interface, EventdPluginSimpleFunc callback) { interface->stop = callback; }

/**
 * EventdPluginCommandStatus:
 * @EVENTD_PLUGIN_COMMAND_STATUS_OK: No error.
 * @EVENTD_PLUGIN_COMMAND_STATUS_COMMAND_ERROR: A user error occurred in a command (e.g., wrong or invalid arguments).
 * @EVENTD_PLUGIN_COMMAND_STATUS_EXEC_ERROR: An error occurred executing a command (e.g., failed to open a socket).
 */

/**
 * EventdPluginControlCommandFunc:
 * @context: the plugin-specific context
 * @argc: argument count of the plugin's command line
 * @argv: (transfer none) (array zero-terminated=1): arguments count of the plugin's command line
 * @status: (out) (transfer none): text to display to the user
 *
 * Callback which executes a subcommand for the plugin. The callback should
 * fill in @status with any messages to display to the user.
 *
 * Returns: the status result of the command as an #EventdPluginCommandStatus
 */

/**
 * eventd_plugin_interface_add_control_command_callback:
 * @interface: an #EventdPluginInterface
 * @callback: (scope async): a function to call when a plugin subcommand is called
 *
 * This callback is used to handle `eventdctl <pluginid> <command> <args>`.
 */
EVENTD_EXPORT void eventd_plugin_interface_add_control_command_callback(EventdPluginInterface *interface, EventdPluginControlCommandFunc callback) { interface->control_command = callback; }

/**
 * EventdPluginGlobalParseFunc:
 * @context: the plugin-specific context
 * @key_file (transfer none): the global configuration
 *
 * This callback handles global configuration parsing for the plugin.
 */

/**
 * EventdPluginEventParseFunc:
 * @context: the plugin-specific context
 * @config_id: the name of the configuration
 * @key_file (transfer none): the global configuration
 *
 * This callback handles event-specific configuration parsing.
 */

/**
 * eventd_plugin_interface_add_config_init_callback:
 * @interface: an #EventdPluginInterface
 * @callback: (scope async): a function to call when pigs fly
 *
 * This callback is being removed in the future.
 *
 * TODO: graft wings on pigs
 */
EVENTD_EXPORT void eventd_plugin_interface_add_config_init_callback(EventdPluginInterface *interface, EventdPluginSimpleFunc callback) { interface->config_init = callback; }
/**
 * eventd_plugin_interface_add_global_parse_callback:
 * @interface: an #EventdPluginInterface
 * @callback: (scope async): a function to call during global configuration parsing
 *
 * This callback is used to handle global configuration (as opposed to
 * per-event configuration).
 */
EVENTD_EXPORT void eventd_plugin_interface_add_global_parse_callback(EventdPluginInterface *interface, EventdPluginGlobalParseFunc callback) { interface->global_parse = callback; }
/**
 * eventd_plugin_interface_add_event_parse_callback:
 * @interface: an #EventdPluginInterface
 * @callback: (scope async): a function to call during event-specific parsing
 *
 * This callback is used to handle event-specific configuration (e.g., how to
 * handle different urgencies of events).
 */
EVENTD_EXPORT void eventd_plugin_interface_add_event_parse_callback(EventdPluginInterface *interface, EventdPluginEventParseFunc callback) { interface->event_parse = callback; }
/**
 * eventd_plugin_interface_add_config_reset_callback:
 * @interface: an #EventdPluginInterface
 * @callback: (scope async): a function to call when reloading the configuration
 *
 * This callback is called before reloading the configuration and shutdown.
 */
EVENTD_EXPORT void eventd_plugin_interface_add_config_reset_callback(EventdPluginInterface *interface, EventdPluginSimpleFunc callback) { interface->config_reset = callback; }

/**
 * EventdPluginEventDispatchFunc:
 * @context: the plugin-specific context
 * @config_id: the name of the configuration
 * @event: (transfer none): the dispatched event
 *
 * This callback handles incoming events.
 */

/**
 * eventd_plugin_interface_add_event_action_callback:
 * @interface: an #EventdPluginInterface
 * @callback: (scope async): a function to call when an event occurs
 *
 * This callback is used to handle events and act upon them.
 *
 * Note that currently, this is called for every event; in the future, it will
 * only be called for events which have configuration for the plugin.
 */
EVENTD_EXPORT void eventd_plugin_interface_add_event_action_callback(EventdPluginInterface *interface, EventdPluginEventDispatchFunc callback) { interface->event_action = callback; }
