/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2017 Quentin "Sardem FF7" Glidic
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

#include <string.h>

#include <glib.h>
#include <gio/gio.h>

#include "eventd-plugin.h"
#include "libeventd-helpers-config.h"

struct _EventdPluginContext {
    gchar *runtime_dir;
    GSList *actions;
};

struct _EventdPluginAction {
    FormatString *file;
    FormatString *string;
    gboolean truncate;
};

static void
_eventd_file_action_free(gpointer data)
{
    EventdPluginAction *action = data;

    evhelpers_format_string_unref(action->string);
    evhelpers_format_string_unref(action->file);

    g_slice_free(EventdPluginAction, action);
}

/*
 * Initialization interface
 */

static EventdPluginContext *
_eventd_file_init(EventdPluginCoreContext *core)
{
    EventdPluginContext *context;

    context = g_new0(EventdPluginContext, 1);

    context->runtime_dir = g_build_filename(g_get_user_runtime_dir(), PACKAGE_NAME, NULL);

    return context;
}

static void
_eventd_file_uninit(EventdPluginContext *context)
{
    g_free(context->runtime_dir);

    g_free(context);
}


/*
 * Configuration interface
 */

static EventdPluginAction *
_eventd_file_action_parse(EventdPluginContext *context, GKeyFile *config_file)
{
    gboolean disable = FALSE;
    FormatString *file = NULL;
    FormatString *string = NULL;
    gboolean truncate = FALSE;

    if ( ! g_key_file_has_group(config_file, "FileWrite") )
        return NULL;

    if ( evhelpers_config_key_file_get_boolean(config_file, "FileWrite", "Disable", &disable) < 0 )
        return NULL;

    if ( disable )
        return NULL;

    if ( evhelpers_config_key_file_get_format_string(config_file, "FileWrite", "File", &file) != 0 )
        goto fail;

    if ( evhelpers_config_key_file_get_format_string(config_file, "FileWrite", "String", &string) != 0 )
        goto fail;

    if ( evhelpers_config_key_file_get_boolean(config_file, "FileWrite", "Truncate", &truncate) < 0 )
        goto fail;

    EventdPluginAction *action;
    action = g_slice_new(EventdPluginAction);
    action->file = file;
    action->string = string;
    action->truncate = truncate;

    context->actions = g_slist_prepend(context->actions, action);

    return action;

fail:
    evhelpers_format_string_unref(string);
    evhelpers_format_string_unref(file);
    return NULL;
}

static void
_eventd_file_config_reset(EventdPluginContext *context)
{
    g_slist_free_full(context->actions, _eventd_file_action_free);
    context->actions = NULL;
}


/*
 * Event action interface
 */

static void
_eventd_file_replace_fallback_callback(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    g_file_replace_contents_finish(G_FILE(obj), res, NULL, NULL);
    g_object_unref(obj);
    g_free(user_data);
}

static void
_eventd_file_write_callback(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    g_output_stream_write_all_finish(G_OUTPUT_STREAM(obj), res, NULL, NULL);
    g_object_unref(obj);
    g_free(user_data);
}

static void
_eventd_file_write(GFile *file, gchar *string, GAsyncResult *res, gboolean truncate)
{
    GError *error = NULL;
    gchar *path;
    GFileOutputStream *stream;

    path = g_file_get_path(file);
    stream = g_file_append_to_finish(file, res, &error);

    if ( stream == NULL )
    {
        g_warning("Could not write to file '%s': %s", path, error->message);
        g_free(string);
        goto out;
    }

    if ( truncate )
    {
        if ( ( ! g_seekable_can_truncate(G_SEEKABLE(stream)) ) || ( ! g_seekable_truncate(G_SEEKABLE(stream), 0, NULL, &error) ) )
        {
            g_warning("Could not truncate file '%s'%s%s", path, ( error != NULL ) ? ": " : "", ( error != NULL ) ? error->message : "");
            g_file_replace_contents_async(g_object_ref(file), string, strlen(string), NULL, FALSE, G_FILE_CREATE_NONE, NULL, _eventd_file_replace_fallback_callback, string);
            goto out;
        }
    }

    g_output_stream_write_all_async(G_OUTPUT_STREAM(stream), string, strlen(string), G_PRIORITY_DEFAULT, NULL, _eventd_file_write_callback, string);

out:
    g_clear_error(&error);
    g_free(path);
    g_object_unref(file);
}

static void
_eventd_file_truncate_callback(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    _eventd_file_write(G_FILE(obj), user_data, res, TRUE);
}

static void
_eventd_file_append_callback(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    _eventd_file_write(G_FILE(obj), user_data, res, FALSE);
}

static void
_eventd_file_event_action(EventdPluginContext *context, EventdPluginAction *action, EventdEvent *event)
{
    gchar *uri = NULL;
    GFile *file;
    gchar *string = NULL;

    uri = evhelpers_format_string_get_string(action->file, event, NULL, NULL);
    file = g_file_new_for_commandline_arg_and_cwd(uri, context->runtime_dir);
    g_free(uri);

    string = evhelpers_format_string_get_string(action->string, event, NULL, NULL);

    g_file_append_to_async(file, G_FILE_CREATE_NONE, G_PRIORITY_DEFAULT, NULL, action->truncate ? _eventd_file_truncate_callback : _eventd_file_append_callback, string);
}


/*
 * Plugin interface
 */

EVENTD_EXPORT const gchar *eventd_plugin_id = "file";
EVENTD_EXPORT const gboolean eventd_plugin_system_mode_support = TRUE;
EVENTD_EXPORT
void
eventd_plugin_get_interface(EventdPluginInterface *interface)
{
    eventd_plugin_interface_add_init_callback(interface, _eventd_file_init);
    eventd_plugin_interface_add_uninit_callback(interface, _eventd_file_uninit);

    eventd_plugin_interface_add_action_parse_callback(interface, _eventd_file_action_parse);
    eventd_plugin_interface_add_config_reset_callback(interface, _eventd_file_config_reset);

    eventd_plugin_interface_add_event_action_callback(interface, _eventd_file_event_action);
}
