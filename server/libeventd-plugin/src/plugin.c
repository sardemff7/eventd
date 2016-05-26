/*
 * libeventd-plugin - Library to implement an eventd plugin
 *
 * Copyright © 2011-2016 Quentin "Sardem FF7" Glidic
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

#include <eventd-plugin.h>
#include <eventd-plugin-private.h>

/**
 * SECTION: Plugins
 *
 * Eventd plugins are used for distributing events over various channels.
 * A plugin should have a symbol named eventd_plugin_id which is a name for the
 * plugin. This id is prefixed with "eventd-" for official plugins maitained
 * along with eventd. Unofficial plugins must not use this prefix.
 * In addition, an eventd_plugin_get_interface function matching
 * #EventdPluginGetInterfaceFunc is called to initialize the plugin.
 */

/**
 * EventdPluginCoreContext:
 *
 * An opaque pointer eventd uses to keep track of plugins.
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
 * EventdPluginAction:
 *
 * A plugin-defined structure which holds the necessary data to perform an action.
 */

/**
 * EventdPluginSimpleFunc:
 * @context: the plugin-specific context
 */

/**
 * EventdPluginInitFunc:
 * @core: the context to use for the core functions
 *
 * Returns: (transfer full): a plugin-specific context object
 */

/**
 * EventdPluginCommandStatus:
 * @EVENTD_PLUGIN_COMMAND_STATUS_OK: The command was successful.
 * @EVENTD_PLUGIN_COMMAND_STATUS_COMMAND_ERROR: Plugin command error. The plugin command is unknown, or miss an argument.
 * @EVENTD_PLUGIN_COMMAND_STATUS_EXEC_ERROR: Plugin command execution error. The command could not succeed, see <command>eventdctl</command> output for details.
 * @EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_1: Plugin custom status.
 * @EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_2: Plugin custom status.
 * @EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_3: Plugin custom status.
 * @EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_4: Plugin custom status.
 * @EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_5: Plugin custom status.
 * @EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_6: Plugin custom status.
 * @EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_7: Plugin custom status.
 * @EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_8: Plugin custom status.
 * @EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_9: Plugin custom status.
 * @EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_10: Plugin custom status.
 */

EVENTD_EXPORT
GType
eventd_plugin_command_status_get_type(void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if ( g_once_init_enter(&g_define_type_id__volatile) )
    {
        static const GEnumValue values[] = {
            { EVENTD_PLUGIN_COMMAND_STATUS_OK,            "EVENTD_PLUGIN_COMMAND_STATUS_OK",            "ok" },
            { EVENTD_PLUGIN_COMMAND_STATUS_COMMAND_ERROR, "EVENTD_PLUGIN_COMMAND_STATUS_COMMAND_ERROR", "command-error" },
            { EVENTD_PLUGIN_COMMAND_STATUS_EXEC_ERROR,    "EVENTD_PLUGIN_COMMAND_STATUS_EXEC_ERROR",    "exec-error" },
            { EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_1,      "EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_1",      "custom-1" },
            { EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_2,      "EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_2",      "custom-2" },
            { EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_3,      "EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_3",      "custom-3" },
            { EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_4,      "EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_4",      "custom-4" },
            { EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_5,      "EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_5",      "custom-5" },
            { EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_6,      "EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_6",      "custom-6" },
            { EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_7,      "EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_7",      "custom-7" },
            { EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_8,      "EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_8",      "custom-8" },
            { EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_9,      "EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_9",      "custom-9" },
            { EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_10,     "EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_10",     "custom-10" },
            { 0, NULL, NULL }
        };
        GType g_define_type_id = g_enum_register_static(g_intern_static_string("EventdPluginCommandStatus"), values);
        g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}

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
 * This callback is used to handle `eventdctl &lt;pluginid&gt; &lt;command&gt; &lt;args&gt;`.
 */
EVENTD_EXPORT void eventd_plugin_interface_add_control_command_callback(EventdPluginInterface *interface, EventdPluginControlCommandFunc callback) { interface->control_command = callback; }

/**
 * EventdPluginGlobalParseFunc:
 * @context: the plugin-specific context
 * @key_file (transfer none): the global configuration file
 *
 * This callback handles global configuration parsing for the plugin.
 */

/**
 * EventdPluginActionParseFunc:
 * @context: the plugin-specific context
 * @key_file (transfer none): the action configuration file
 *
 * This callback handles action configuration parsing.
 *
 * Returns: (transfer full) (nullable): an #EventdPluginAction or %NULL
 */

/**
 * eventd_plugin_interface_add_global_parse_callback:
 * @interface: an #EventdPluginInterface
 * @callback: (scope async): a function to call during global configuration parsing
 *
 * This callback is used to handle global configuration (as opposed to
 * action configuration).
 */
EVENTD_EXPORT void eventd_plugin_interface_add_global_parse_callback(EventdPluginInterface *interface, EventdPluginGlobalParseFunc callback) { interface->global_parse = callback; }
/**
 * eventd_plugin_interface_add_action_parse_callback:
 * @interface: an #EventdPluginInterface
 * @callback: (scope async): a function to call during action configuration parsing
 *
 * This callback is used to handle action configuration (e.g., how to
 * handle different urgencies of events).
 */
EVENTD_EXPORT void eventd_plugin_interface_add_action_parse_callback(EventdPluginInterface *interface, EventdPluginActionParseFunc callback) { interface->action_parse = callback; }
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
 * @event: (transfer none): the dispatched event
 *
 * This callback handles events.
 */

/**
 * EventdPluginEventActionFunc:
 * @context: the plugin-specific context
 * @action: (transfer none): the plugin action
 * @event: (transfer none): the dispatched event
 *
 * This callback handles actions.
 */

/**
 * eventd_plugin_interface_add_event_dispatch_callback:
 * @interface: an #EventdPluginInterface
 * @callback: (scope async): a function to call when an event occurs
 *
 * This callback is called *once* per known event.
 *
 * This callback is intended for plugins needing to relay event to clients
 * using SUBSCRIBE. It should not be abused for anything else.
 */
EVENTD_EXPORT void eventd_plugin_interface_add_event_dispatch_callback(EventdPluginInterface *interface, EventdPluginEventDispatchFunc callback) { interface->event_dispatch = callback; }

/**
 * eventd_plugin_interface_add_event_action_callback:
 * @interface: an #EventdPluginInterface
 * @callback: (scope async): a function to call when triggering an action
 *
 * This callback is called whenever an event triggers an action created by the
 * plugin.
 */
EVENTD_EXPORT void eventd_plugin_interface_add_event_action_callback(EventdPluginInterface *interface, EventdPluginEventActionFunc callback) { interface->event_action = callback; }
