/*
 * libeventd-plugins-helper - Internal helper to deal with plugins
 *
 * Copyright © 2011 Quentin "Sardem FF7" Glidic
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

#include <glib.h>
#include <gmodule.h>

#include <eventd-plugin.h>
#include "plugins-helper.h"

static void
eventd_plugins_helper_load_dir(GList **plugins, const gchar *plugins_dir_name, gpointer user_data)
{
    GError *error;
    GDir *plugins_dir;
    gchar *file;

    if ( ! g_module_supported() )
    {
        g_warning("Couldn’t load plugins: %s", g_module_error());
        return;
    }

    #if DEBUG
    g_debug("Scannig plugins dir: %s", plugins_dir_name);
    #endif /* DEBUG */

    plugins_dir = g_dir_open(plugins_dir_name, 0, &error);
    if ( ! plugins_dir )
    {
        g_warning("Can’t read the plugins directory: %s", error->message);
        g_clear_error(&error);
        return;
    }

    while ( ( file = (gchar *)g_dir_read_name(plugins_dir) ) != NULL )
    {
        gchar *full_filename;
        EventdPlugin *plugin;
        EventdPluginGetInfoFunc get_info;
        void *module;

        full_filename = g_build_filename(plugins_dir_name, file, NULL);

        if ( g_file_test(full_filename, G_FILE_TEST_IS_DIR) )
            goto next;

        module = g_module_open(full_filename, G_MODULE_BIND_LAZY|G_MODULE_BIND_LOCAL);
        if ( module == NULL )
        {
            g_warning("Couldn’t load module '%s': %s", file, g_module_error());
            goto next;
        }

        if ( ! g_module_symbol(module, "eventd_plugin_get_info", (void **)&get_info) )
            goto next;

        #if DEBUG
        g_debug("Loading plugin '%s'", file);
        #endif /* ! DEBUG */

        plugin = g_new0(EventdPlugin, 1);
        plugin->module = module;
        get_info(plugin);

        if ( plugin->start != NULL )
            plugin->start(user_data);

        *plugins = g_list_prepend(*plugins, plugin);

    next:
        g_free(full_filename);
    }
}

void
eventd_plugins_helper_load(GList **plugins, const gchar *plugins_subdir, gpointer user_data)
{
    gchar *plugins_dir;

    if ( plugins == NULL )
        return;

    plugins_dir = g_build_filename(LIBDIR, PACKAGE_NAME, plugins_subdir, NULL);
    if ( g_file_test(plugins_dir, G_FILE_TEST_IS_DIR) )
        eventd_plugins_helper_load_dir(plugins, plugins_dir, user_data);
    g_free(plugins_dir);

    plugins_dir = g_build_filename(DATADIR, PACKAGE_NAME, plugins_subdir, NULL);
    if ( g_file_test(plugins_dir, G_FILE_TEST_IS_DIR) )
        eventd_plugins_helper_load_dir(plugins, plugins_dir, user_data);
    g_free(plugins_dir);

    plugins_dir = g_build_filename(g_get_user_data_dir(), PACKAGE_NAME, plugins_subdir, NULL);
    if ( g_file_test(plugins_dir, G_FILE_TEST_IS_DIR) )
        eventd_plugins_helper_load_dir(plugins, plugins_dir, user_data);
    g_free(plugins_dir);
}

static void
plugin_free(EventdPlugin *plugin)
{
    if ( plugin->stop != NULL )
        plugin->stop();
    g_module_close(plugin->module);
    g_free(plugin);
}

void
eventd_plugins_helper_unload(GList **plugins)
{
    if ( ( plugins == NULL ) || ( *plugins == NULL ) )
        return;
    g_list_free_full(*plugins, (GDestroyNotify)plugin_free);
    *plugins = NULL;
}

void
eventd_plugins_helper_foreach(GList *plugins, GFunc func, gpointer user_data)
{
    if ( plugins == NULL )
        return;
    g_list_foreach(plugins, func, user_data);
}


void
eventd_plugins_helper_config_init(EventdPlugin *plugin, gpointer data)
{
    plugin->config_init();
}

void
eventd_plugins_helper_config_init_all(GList *plugins)
{
    eventd_plugins_helper_foreach(plugins, (GFunc)eventd_plugins_helper_config_init, NULL);
}

static void
eventd_plugins_helper_config_clean(EventdPlugin *plugin, gpointer data)
{
    plugin->config_clean();
}

void
eventd_plugins_helper_config_clean_all(GList *plugins)
{
    eventd_plugins_helper_foreach(plugins, (GFunc)eventd_plugins_helper_config_clean, NULL);
}

typedef struct {
    const gchar *type;
    const gchar *event;
    GKeyFile *config_file;
} EventdEventParseData;

static void
eventd_plugins_helper_event_parse(EventdPlugin *plugin, EventdEventParseData *data)
{
    plugin->event_parse(data->type, data->event, data->config_file);
}

void eventd_plugins_helper_event_parse_all(GList *plugins, const gchar *type, const gchar *event, GKeyFile *config_file)
{
    EventdEventParseData data = {
        .type = type,
        .event = event,
        .config_file = config_file,
    };
    eventd_plugins_helper_foreach(plugins, (GFunc)eventd_plugins_helper_event_parse, &data);
}

typedef struct {
    const gchar *client_type;
    const gchar *client_name;

    const gchar *event_type;
    const gchar *event_name;
    const GHashTable *event_data;
} EventdEventActionData;

static void
eventd_plugins_helper_event_action(EventdPlugin *plugin, EventdEventActionData *data)
{
    plugin->event_action(data->client_type, data->client_name, data->event_type, data->event_name, data->event_data);
}

void
eventd_plugins_helper_event_action_all(GList *plugins, const gchar *client_type, const gchar *client_name, const gchar *event_type, const gchar *event_name, const GHashTable *event_data)
{
    EventdEventActionData data = {
        .client_type = client_type,
        .client_name = client_name,

        .event_type = event_type,
        .event_name = event_name,
        .event_data = event_data
    };
    eventd_plugins_helper_foreach(plugins, (GFunc)eventd_plugins_helper_event_action, &data);
}
