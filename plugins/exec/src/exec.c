/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2016 Quentin "Sardem FF7" Glidic
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

#include "config.h"

#include <glib.h>

#include "eventd-plugin.h"
#include "libeventd-helpers-config.h"

struct _EventdPluginContext {
    GSList *actions;
};

struct _EventdPluginAction {
    FormatString *command;
};

static void
_eventd_exec_action_free(gpointer data)
{
    EventdPluginAction *action = data;

    evhelpers_format_string_unref(action->command);

    g_slice_free(EventdPluginAction, action);
}

/*
 * Initialization interface
 */

static EventdPluginContext *
_eventd_exec_init(EventdPluginCoreContext *core)
{
    EventdPluginContext *context;

    context = g_new0(EventdPluginContext, 1);

    return context;
}

static void
_eventd_exec_uninit(EventdPluginContext *context)
{
    g_free(context);
}


/*
 * Configuration interface
 */

static EventdPluginAction *
_eventd_exec_action_parse(EventdPluginContext *context, GKeyFile *config_file)
{
    gboolean disable;
    FormatString *command = NULL;

    if ( ! g_key_file_has_group(config_file, "Exec") )
        return NULL;

    if ( evhelpers_config_key_file_get_boolean(config_file, "Exec", "Disable", &disable) < 0 )
        return NULL;

    if ( disable )
        return NULL;

    if ( evhelpers_config_key_file_get_format_string(config_file, "Exec", "Command", &command) < 0 )
        return NULL;

    EventdPluginAction *action;
    action = g_slice_new(EventdPluginAction);
    action->command = command;

    context->actions = g_slist_prepend(context->actions, action);

    return action;
}

static void
_eventd_exec_config_reset(EventdPluginContext *context)
{
    g_slist_free_full(context->actions, _eventd_exec_action_free);
    context->actions = NULL;
}


/*
 * Event action interface
 */

static void
_eventd_exec_event_action(EventdPluginContext *context, EventdPluginAction *action, EventdEvent *event)
{
    gchar *cmd;
    GError *error = NULL;

    cmd = evhelpers_format_string_get_string(action->command, event, NULL, NULL);

    if ( ! g_spawn_command_line_async(cmd, &error) )
    {
        g_warning("Couldn't spawn '%s': %s", cmd, error->message);
        g_clear_error(&error);
    }

    g_free(cmd);
}


/*
 * Plugin interface
 */

EVENTD_EXPORT const gchar *eventd_plugin_id = "exec";
EVENTD_EXPORT const gboolean eventd_plugin_system_mode_support = TRUE;
EVENTD_EXPORT
void
eventd_plugin_get_interface(EventdPluginInterface *interface)
{
    eventd_plugin_interface_add_init_callback(interface, _eventd_exec_init);
    eventd_plugin_interface_add_uninit_callback(interface, _eventd_exec_uninit);

    eventd_plugin_interface_add_action_parse_callback(interface, _eventd_exec_action_parse);
    eventd_plugin_interface_add_config_reset_callback(interface, _eventd_exec_config_reset);

    eventd_plugin_interface_add_event_action_callback(interface, _eventd_exec_event_action);
}
