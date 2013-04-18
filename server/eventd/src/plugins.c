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

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <glib.h>
#include <glib-compat.h>
#include <glib-object.h>
#include <gmodule.h>

#include <eventd-plugin.h>
#include <eventd-plugin-interfaces.h>

#include <eventdctl.h>

#include "plugins.h"

typedef struct {
    GModule *module;
    EventdPluginContext *context;
    EventdPluginInterface interface;
} EventdPlugin;

static GHashTable *plugins = NULL;


static void
_eventd_plugins_load_dir(EventdCoreContext *core, EventdCoreInterface *interface, const gchar *plugins_dir_name,  gchar **whitelist,  gchar **blacklist)
{
    GError *error;
    GDir *plugins_dir;
    const gchar *file;


#ifdef DEBUG
    g_debug("Scannig plugins dir: %s", plugins_dir_name);
#endif /* DEBUG */

    plugins_dir = g_dir_open(plugins_dir_name, 0, &error);
    if ( ! plugins_dir )
    {
        g_warning("Couldn't read the plugins directory: %s", error->message);
        g_clear_error(&error);
        return;
    }

    while ( ( file = g_dir_read_name(plugins_dir) ) != NULL )
    {
        gchar *full_filename;
        EventdPlugin *plugin;
        const gchar **id;
        EventdPluginGetInterfaceFunc get_interface;
        GModule *module;

        if ( ! g_str_has_suffix(file, G_MODULE_SUFFIX) )
            continue;

        if ( whitelist != NULL )
        {
            gboolean whitelisted = FALSE;
            gchar **wname;
            for ( wname = whitelist ; *wname != NULL ; ++wname )
            {
                if ( g_str_has_prefix(file, *wname) )
                {
                    whitelisted = TRUE;
                    break;
                }
            }
            if ( ! whitelisted )
                continue;
        }

        if ( blacklist != NULL )
        {
            gboolean blacklisted = FALSE;
            gchar **bname;
            for ( bname = blacklist ; *bname != NULL ; ++bname )
            {
                if ( g_str_has_prefix(file, *bname) )
                {
                    blacklisted = TRUE;
                    break;
                }
            }
            if ( blacklisted )
                continue;
        }

        full_filename = g_build_filename(plugins_dir_name, file, NULL);

        if ( g_file_test(full_filename, G_FILE_TEST_IS_DIR) )
        {
            g_free(full_filename);
            continue;
        }

        module = g_module_open(full_filename, G_MODULE_BIND_LAZY|G_MODULE_BIND_LOCAL);
        if ( module == NULL )
        {
            g_warning("Couldn't load module '%s': %s", file, g_module_error());
            g_free(full_filename);
            continue;
        }
        g_free(full_filename);

        if ( ! g_module_symbol(module, "eventd_plugin_id", (void **)&id) )
            continue;

        if ( id == NULL )
        {
            g_warning("Plugin '%s' must define eventd_plugin_id", file);
            continue;
        }

        if ( g_hash_table_contains(plugins, *id) )
        {
#ifdef DEBUG
            g_debug("Plugin '%s' with id '%s' already loaded", file, *id);
#endif /* ! DEBUG */
            continue;
        }

        if ( ! g_module_symbol(module, "eventd_plugin_get_interface", (void **)&get_interface) )
            continue;

#ifdef DEBUG
        g_debug("Loading plugin '%s': %s", file, *id);
#endif /* ! DEBUG */

        plugin = g_new0(EventdPlugin, 1);
        plugin->module = module;
        get_interface(&plugin->interface);

        if ( plugin->interface.init != NULL )
        {
            plugin->context = plugin->interface.init(core, interface);
            if ( plugin->context == NULL )
            {
                g_warning("Couldn't load plugin '%s'", file);
                g_module_close(plugin->module);
                g_free(plugin);
                continue;
            }
        }

        g_hash_table_insert(plugins, (gpointer) *id, plugin);
    }
    g_dir_close(plugins_dir);
}

static void
_eventd_plugins_plugin_free(gpointer data)
{
    EventdPlugin *plugin = data;

    if ( plugin->interface.uninit != NULL )
        plugin->interface.uninit(plugin->context);
    g_module_close(plugin->module);
    g_free(plugin);
}

void
eventd_plugins_load(EventdCoreContext *core, EventdCoreInterface *interface)
{
    const gchar *env_whitelist;
    const gchar *env_blacklist;
    const gchar *env_base_dir;
    gchar **whitelist = NULL;
    gchar **blacklist = NULL;
    gchar *plugins_dir;

    if ( ! g_module_supported() )
    {
        g_warning("Couldn't load plugins: %s", g_module_error());
        return;
    }

    plugins = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, _eventd_plugins_plugin_free);

    env_whitelist = g_getenv("EVENTD_PLUGINS_WHITELIST");
    if ( env_whitelist != NULL )
        whitelist = g_strsplit(env_whitelist, ",", 0);

    env_blacklist = g_getenv("EVENTD_PLUGINS_BLACKLIST");
    if ( env_blacklist != NULL )
        blacklist = g_strsplit(env_blacklist, ",", 0);

    env_base_dir = g_getenv("EVENTD_PLUGINS_DIR");
    if ( env_base_dir != NULL )
    {
        if ( env_base_dir[0] == '~' )
            plugins_dir = g_build_filename(g_get_home_dir(), env_base_dir+2, NULL);
        else
            plugins_dir = g_build_filename(env_base_dir,  NULL);

        if ( g_file_test(plugins_dir, G_FILE_TEST_IS_DIR) )
            _eventd_plugins_load_dir(core, interface, plugins_dir, whitelist, blacklist);
        g_free(plugins_dir);
    }

    plugins_dir = g_build_filename(g_get_user_data_dir(), PACKAGE_NAME, "plugins", NULL);
    if ( g_file_test(plugins_dir, G_FILE_TEST_IS_DIR) )
        _eventd_plugins_load_dir(core, interface, plugins_dir, whitelist, blacklist);
    g_free(plugins_dir);

    plugins_dir = g_build_filename(DATADIR, PACKAGE_NAME, "plugins", NULL);
    if ( g_file_test(plugins_dir, G_FILE_TEST_IS_DIR) )
        _eventd_plugins_load_dir(core, interface, plugins_dir, whitelist, blacklist);
    g_free(plugins_dir);

    plugins_dir = g_build_filename(LIBDIR, PACKAGE_NAME, "plugins", NULL);
    if ( g_file_test(plugins_dir, G_FILE_TEST_IS_DIR) )
        _eventd_plugins_load_dir(core, interface, plugins_dir, whitelist, blacklist);
    g_free(plugins_dir);

    g_strfreev(blacklist);
    g_strfreev(whitelist);
}

void
eventd_plugins_unload()
{
    if ( plugins == NULL )
        return;
    g_hash_table_unref(plugins);
    plugins = NULL;
}

void
eventd_plugins_add_option_group_all(GOptionContext *option_context)
{
    GHashTableIter iter;
    const gchar *id;
    EventdPlugin *plugin;
    g_hash_table_iter_init(&iter, plugins);
    while ( g_hash_table_iter_next(&iter, (gpointer *)&id, (gpointer *)&plugin) )
    {
        if ( plugin->interface.get_option_group != NULL )
        {
            GOptionGroup *option_group;

            option_group = plugin->interface.get_option_group(plugin->context);
            if ( option_group != NULL )
                g_option_context_add_group(option_context, option_group);
        }
    }
}

void
eventd_plugins_start_all()
{
    GHashTableIter iter;
    const gchar *id;
    EventdPlugin *plugin;
    g_hash_table_iter_init(&iter, plugins);
    while ( g_hash_table_iter_next(&iter, (gpointer *)&id, (gpointer *)&plugin) )
    {
        if ( plugin->interface.start != NULL )
            plugin->interface.start(plugin->context);
    }
}

void
eventd_plugins_stop_all()
{
    GHashTableIter iter;
    const gchar *id;
    EventdPlugin *plugin;
    g_hash_table_iter_init(&iter, plugins);
    while ( g_hash_table_iter_next(&iter, (gpointer *)&id, (gpointer *)&plugin) )
    {
        if ( plugin->interface.stop != NULL )
            plugin->interface.stop(plugin->context);
    }
}

EventdctlReturnCode
eventd_plugins_control_command(const gchar *id, const gchar *command, const gchar *args, gchar **status)
{
    const gchar *eid;
    EventdPlugin *plugin;
    EventdctlReturnCode r;;

    eid = g_str_has_prefix(id, "eventd-") ? ( id + strlen("eventd-") ) : id;

    plugin = g_hash_table_lookup(plugins, id);
    if ( plugin == NULL )
    {
        *status = g_strdup_printf("No such plugin '%s'", eid);
        r = EVENTCTL_RETURN_CODE_PLUGIN_ERROR;
    }
    else if ( plugin->interface.control_command == NULL )
    {
        *status = g_strdup_printf("Plugin '%s' does not support control commands", eid);
        r = EVENTCTL_RETURN_CODE_PLUGIN_ERROR;
    }
    else
    {
        *status = plugin->interface.control_command(plugin->context, command, args);
        r = EVENTCTL_RETURN_CODE_OK;
    }

    return r;
}

void
eventd_plugins_config_reset_all()
{
    GHashTableIter iter;
    const gchar *id;
    EventdPlugin *plugin;
    g_hash_table_iter_init(&iter, plugins);
    while ( g_hash_table_iter_next(&iter, (gpointer *)&id, (gpointer *)&plugin) )
    {
        if ( plugin->interface.config_reset != NULL )
            plugin->interface.config_reset(plugin->context);
    }
}

void
eventd_plugins_global_parse_all(GKeyFile *config_file)
{
    GHashTableIter iter;
    const gchar *id;
    EventdPlugin *plugin;
    g_hash_table_iter_init(&iter, plugins);
    while ( g_hash_table_iter_next(&iter, (gpointer *)&id, (gpointer *)&plugin) )
    {
        if ( plugin->interface.global_parse != NULL )
            plugin->interface.global_parse(plugin->context, config_file);
    }
}

void
eventd_plugins_event_parse_all(const gchar *event_id, GKeyFile *config_file)
{
    GHashTableIter iter;
    const gchar *id;
    EventdPlugin *plugin;
    g_hash_table_iter_init(&iter, plugins);
    while ( g_hash_table_iter_next(&iter, (gpointer *)&id, (gpointer *)&plugin) )
    {
        if ( plugin->interface.event_parse != NULL )
            plugin->interface.event_parse(plugin->context, event_id, config_file);
    }
}

void
eventd_plugins_event_action_all(const gchar *config_id, EventdEvent *event)
{
    GHashTableIter iter;
    const gchar *id;
    EventdPlugin *plugin;
    g_hash_table_iter_init(&iter, plugins);
    while ( g_hash_table_iter_next(&iter, (gpointer *)&id, (gpointer *)&plugin) )
    {
        if ( plugin->interface.event_action != NULL )
            plugin->interface.event_action(plugin->context, config_id, event);
    }
}
