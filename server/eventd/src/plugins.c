/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2018 Quentin "Sardem FF7" Glidic
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
#include <glib-object.h>
#include <gmodule.h>

#include "eventd-plugin.h"
#include "eventd-plugin-private.h"

#include "libeventd-helpers-dirs.h"

#include "eventdctl.h"

#include "types.h"
#include "evp/evp.h"
#include "relay/relay.h"
#include "relay/server.h"
#include "sd-modules.h"

typedef struct {
    GModule *module;
    EventdPluginContext *context;
    EventdPluginInterface interface;
} EventdPlugin;

typedef struct {
    EventdPlugin *plugin;
    EventdPluginAction *action;
} EventdPluginsAction;

static EventdEvpContext *evp = NULL;
static EventdRelayContext *relay = NULL;
static GHashTable *plugins = NULL;


static void
_eventd_plugins_load_dir(EventdPluginCoreContext *core, gchar *plugins_dir_name, gboolean system_mode, gchar **whitelist, gchar **blacklist)
{
    GError *error;
    GDir *plugins_dir;
    const gchar *file;


    eventd_debug("Scanning plugins dir: %s", plugins_dir_name);

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
        EventdPlugin *plugin = NULL;
        const gchar **id;
        EventdPluginGetInterfaceFunc get_interface;
        GModule *module;

        if ( ! g_str_has_suffix(file, G_MODULE_SUFFIX) )
            continue;

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
            goto next;

        if ( id == NULL )
        {
            g_warning("Plugin '%s' must define eventd_plugin_id", file);
            goto next;
        }

        if ( ( g_strcmp0(*id, "evp") == 0 ) || ( g_strcmp0(*id, "relay") == 0 ) )
            goto next;

        if ( whitelist != NULL )
        {
            gboolean whitelisted = FALSE;
            gchar **wname;
            for ( wname = whitelist ; *wname != NULL ; ++wname )
            {
                if ( g_strcmp0(*id, *wname) == 0 )
                {
                    whitelisted = TRUE;
                    break;
                }
            }
            if ( ! whitelisted )
                goto next;
        }

        if ( blacklist != NULL )
        {
            gboolean blacklisted = FALSE;
            gchar **bname;
            for ( bname = blacklist ; *bname != NULL ; ++bname )
            {
                if ( g_strcmp0(*id, *bname) == 0 )
                {
                    blacklisted = TRUE;
                    break;
                }
            }
            if ( blacklisted )
                goto next;
        }

        if ( g_hash_table_contains(plugins, *id) )
        {
            eventd_debug("Plugin '%s' with id '%s' already loaded", file, *id);
            goto next;
        }

        if ( system_mode )
        {
            gboolean *system_mode_support;
            if ( ( ! g_module_symbol(module, "eventd_plugin_system_mode_support", (gpointer *) &system_mode_support) )
                 || ( ! *system_mode_support ) )
            {
                eventd_debug("Plugin '%s' does not support system mode", *id);
                goto next;
            }
        }

        if ( ! g_module_symbol(module, "eventd_plugin_get_interface", (void **)&get_interface) )
            goto next;

        eventd_debug("Loading plugin '%s': %s", file, *id);

        plugin = g_new0(EventdPlugin, 1);
        plugin->module = module;
        get_interface(&plugin->interface);

        if ( ! (
                ( ( plugin->interface.action_parse == NULL ) && ( plugin->interface.event_action == NULL ) )
                ||
                ( ( plugin->interface.action_parse != NULL ) && ( plugin->interface.event_action != NULL ) )
            ) )
        {
            g_warning("Plugin '%s' should define either both or neither of action_parse/event_action", *id);
            goto next;
        }

        if ( plugin->interface.init != NULL )
        {
            plugin->context = plugin->interface.init(core);
            if ( plugin->context == NULL )
            {
                g_warning("Couldn't load plugin '%s'", *id);
                goto next;
            }
        }

        g_hash_table_insert(plugins, (gpointer) *id, plugin);
        continue;

    next:
        g_module_close(module);
        g_free(plugin);
    }
    g_dir_close(plugins_dir);
    g_free(plugins_dir_name);
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
eventd_plugins_action_free(gpointer data)
{
    g_slice_free(EventdPluginsAction, data);
}

static const EventdSdModuleControlInterface _eventd_plugins_sd_modules_control_interface = {
    .server_has_address = eventd_relay_server_has_address,
    .server_set_address = eventd_relay_server_set_address,
    .server_start = eventd_relay_server_start,
    .server_stop = eventd_relay_server_stop,
};

void
eventd_plugins_load(EventdPluginCoreContext *core, const gchar * const *binds, gboolean enable_relay, gboolean enable_sd_modules, gboolean system_mode)
{
    plugins = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, _eventd_plugins_plugin_free);

    if ( G_LIKELY(g_module_supported()) )
    {
        const gchar *env_whitelist;
        const gchar *env_blacklist;
        gchar **whitelist = NULL;
        gchar **blacklist = NULL;

        env_whitelist = g_getenv("EVENTD_PLUGINS_WHITELIST");
        if ( env_whitelist != NULL )
            whitelist = g_strsplit(env_whitelist, ",", 0);

        env_blacklist = g_getenv("EVENTD_PLUGINS_BLACKLIST");
        if ( env_blacklist != NULL )
            blacklist = g_strsplit(env_blacklist, ",", 0);

        gchar **dirs, **dir;
        dirs = evhelpers_dirs_get_lib("PLUGINS", "plugins");
        for ( dir = dirs ; *dir != NULL ; ++dir )
            _eventd_plugins_load_dir(core, *dir, system_mode, whitelist, blacklist);
        g_free(dirs);

        g_strfreev(blacklist);
        g_strfreev(whitelist);
    }
    else
        g_warning("No loadable module support");


    GList *sockets;

    evp = eventd_evp_init(core, binds, &sockets);
    if ( enable_relay )
        relay = eventd_relay_init(core);

    if ( enable_sd_modules )
        eventd_sd_modules_load(&_eventd_plugins_sd_modules_control_interface, sockets);
    g_list_free_full(sockets, g_object_unref);
}

void
eventd_plugins_unload(void)
{
    if ( plugins == NULL )
        return;

    eventd_sd_modules_unload();

    eventd_relay_uninit(relay);
    relay = NULL;

    eventd_evp_uninit(evp);
    evp = NULL;

    g_hash_table_unref(plugins);
    plugins = NULL;
}

void
eventd_plugins_start_all(void)
{
    eventd_evp_start(evp);
    eventd_relay_start(relay);

    eventd_sd_modules_start();

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
eventd_plugins_stop_all(void)
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

    eventd_sd_modules_stop();
    eventd_relay_stop(relay);
    eventd_evp_stop(evp);
}

EventdctlReturnCode
eventd_plugins_control_command(const gchar *id, guint64 argc, const gchar * const *argv, gchar **status)
{
    EventdPlugin *plugin;
    EventdctlReturnCode r;

    if ( g_strcmp0(id, "relay") == 0 )
        return (EventdctlReturnCode) eventd_relay_control_command(relay, argc, argv, status);

    plugin = g_hash_table_lookup(plugins, id);
    if ( plugin == NULL )
    {
        *status = g_strdup_printf("No such plugin '%s'", id);
        r = EVENTDCTL_RETURN_CODE_PLUGIN_ERROR;
    }
    else if ( plugin->interface.control_command == NULL )
    {
        *status = g_strdup_printf("Plugin '%s' does not support control commands", id);
        r = EVENTDCTL_RETURN_CODE_PLUGIN_ERROR;
    }
    else
        r = (EventdctlReturnCode) plugin->interface.control_command(plugin->context, argc, argv, status);

    return r;
}

void
eventd_plugins_relay_set_certificate(GTlsCertificate *certificate)
{
    eventd_relay_set_certificate(relay, certificate);
}

void
eventd_plugins_config_reset_all(void)
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

    eventd_evp_config_reset(evp);
    eventd_relay_config_reset(relay);
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

    eventd_relay_global_parse(relay, config_file);
    eventd_evp_global_parse(evp, config_file);
}

GList *
eventd_plugins_event_parse_all(GKeyFile *config_file)
{
    GList *actions = NULL;
    GHashTableIter iter;
    const gchar *id;
    EventdPlugin *plugin;
    g_hash_table_iter_init(&iter, plugins);
    while ( g_hash_table_iter_next(&iter, (gpointer *)&id, (gpointer *)&plugin) )
    {
        if ( plugin->interface.action_parse != NULL )
        {
            EventdPluginAction *action;
            action = plugin->interface.action_parse(plugin->context, config_file);
            if ( action == NULL )
                continue;

            EventdPluginsAction *action_;
            action_ = g_slice_new(EventdPluginsAction);
            action_->plugin = plugin;
            action_->action = action;

            actions = g_list_prepend(actions, action_);
        }
    }
    return actions;
}

void
eventd_plugins_event_dispatch_all(EventdEvent *event)
{
    eventd_evp_event_dispatch(evp, event);
    eventd_relay_event_dispatch(relay, event);

    GHashTableIter iter;
    const gchar *id;
    EventdPlugin *plugin;
    g_hash_table_iter_init(&iter, plugins);

    while ( g_hash_table_iter_next(&iter, (gpointer *)&id, (gpointer *)&plugin) )
    {
        if ( plugin->interface.event_dispatch != NULL )
            plugin->interface.event_dispatch(plugin->context, event);
    }
}

void
eventd_plugins_event_action_all(const GList *actions, EventdEvent *event)
{
    for ( ; actions != NULL ; actions = g_list_next(actions) )
    {
        EventdPluginsAction *action = actions->data;
        action->plugin->interface.event_action(action->plugin->context, action->action, event);
    }
}
