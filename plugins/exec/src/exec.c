/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2024 Morgane "Sardem FF7" Glidic
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
    FormatString *stdin_template;
};

static void
_eventd_exec_action_free(gpointer data)
{
    EventdPluginAction *action = data;

    evhelpers_format_string_unref(action->stdin_template);
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
    gboolean disable = FALSE;
    FormatString *command = NULL;
    FormatString *stdin_template = NULL;

    if ( ! g_key_file_has_group(config_file, "Exec") )
        return NULL;

    if ( evhelpers_config_key_file_get_boolean(config_file, "Exec", "Disable", &disable) < 0 )
        return NULL;

    if ( disable )
        return NULL;

    if ( evhelpers_config_key_file_get_format_string(config_file, "Exec", "Command", &command) < 0 )
        return NULL;

    if ( evhelpers_config_key_file_get_template(config_file, "Exec", "StdInTemplate", &stdin_template) < 0 )
        goto fail;

    EventdPluginAction *action;
    action = g_slice_new(EventdPluginAction);
    action->command = command;
    action->stdin_template = stdin_template;

    context->actions = g_slist_prepend(context->actions, action);

    return action;

fail:
    evhelpers_format_string_unref(stdin_template);
    evhelpers_format_string_unref(command);
    return NULL;
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
    gchar **argv;

    gint in_, *in = ( action->stdin_template != NULL ) ? &in_ : NULL;
    GError *error = NULL;

    cmd = evhelpers_format_string_get_string(action->command, event, NULL, NULL);
    if ( ! g_shell_parse_argv(cmd, NULL, &argv, &error) )
    {
        g_warning("Couldn't parse command line '%s': %s", cmd, error->message);
        g_clear_error(&error);
    }
    else if ( ! g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, in, NULL, NULL, &error) )
    {
        g_warning("Couldn't spawn '%s': %s", cmd, error->message);
        g_clear_error(&error);
    }
    else if ( in != NULL )
    {
        GIOChannel *io;
        gchar *data;

#ifdef G_OS_UNIX
        io = g_io_channel_unix_new(*in);
#else /* ! G_OS_UNIX */
        io = g_io_channel_win32_new_fd(*in);
#endif /* ! G_OS_UNIX */
        data = evhelpers_format_string_get_string(action->stdin_template, event, NULL, NULL);

        GIOStatus status;
        gsize bytes_written;
        status = g_io_channel_write_chars(io, data, -1, &bytes_written, &error);
        if ( status != G_IO_STATUS_NORMAL )
        {
            g_warning("Could not write to command stdin '%s': %s", cmd, error->message);
            g_clear_error(&error);
        }
        status = g_io_channel_shutdown(io, TRUE, &error);
        if ( status != G_IO_STATUS_NORMAL )
        {
            g_warning("Could not properly shutdown stdin of '%s': %s", cmd, error->message);
            g_clear_error(&error);
        }
        g_io_channel_unref(io);

        g_free(data);
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
