/*
 * eventd - Small daemon to act on remote or local events
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
#include <libeventd-config.h>

struct _EventdPluginContext {
    GHashTable *events;
};


/*
 * Initialization interface
 */

static EventdPluginContext *
_eventd_exec_init(EventdCoreContext *core, EventdCoreInterface *interface)
{
    EventdPluginContext *context;

    context = g_new0(EventdPluginContext, 1);

    context->events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)libeventd_format_string_unref);

    return context;
}

static void
_eventd_exec_uninit(EventdPluginContext *context)
{
    g_hash_table_unref(context->events);

    g_free(context);
}


/*
 * Configuration interface
 */

static void
_eventd_exec_event_parse(EventdPluginContext *context, const gchar *config_id, GKeyFile *config_file)
{
    gboolean disable;
    LibeventdFormatString *command = NULL;

    if ( ! g_key_file_has_group(config_file, "Exec") )
        return;

    if ( libeventd_config_key_file_get_boolean(config_file, "Exec", "Disable", &disable) < 0 )
        return;

    if ( ! disable )
    {
        if ( libeventd_config_key_file_get_format_string(config_file, "Exec", "Command", &command) < 0 )
            return;
    }

    g_hash_table_insert(context->events, g_strdup(config_id), command);
}

static void
_eventd_exec_config_reset(EventdPluginContext *context)
{
    g_hash_table_remove_all(context->events);
}


/*
 * Event action interface
 */

static void
_eventd_exec_event_action(EventdPluginContext *context, const gchar *config_id, EventdEvent *event)
{
    const LibeventdFormatString *command;
    gchar *cmd;
    GError *error;

    command = g_hash_table_lookup(context->events, config_id);
    if ( command == NULL )
        return;

    cmd = libeventd_format_string_get_string(command, event, NULL, NULL);

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

EVENTD_EXPORT const gchar *eventd_plugin_id = "eventd-exec";
EVENTD_EXPORT
void
eventd_plugin_get_interface(EventdPluginInterface *interface)
{
    libeventd_plugin_interface_add_init_callback(interface, _eventd_exec_init);
    libeventd_plugin_interface_add_uninit_callback(interface, _eventd_exec_uninit);

    libeventd_plugin_interface_add_event_parse_callback(interface, _eventd_exec_event_parse);
    libeventd_plugin_interface_add_config_reset_callback(interface, _eventd_exec_config_reset);

    libeventd_plugin_interface_add_event_action_callback(interface, _eventd_exec_event_action);
}
