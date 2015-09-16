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

#include <config.h>

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>

#include <cairo.h>

#include <eventd-nd-backend.h>

static void
_eventd_nd_backend_free(gpointer data)
{
    EventdNdBackend *backend = data;

    backend->uninit(backend->context);

    g_module_close(backend->module);

    g_free(backend);
}

static void
_eventd_nd_backends_load_dir(EventdNdContext *context, EventdNdInterface *interface, GHashTable *backends, const gchar *backends_dir_name, gchar **whitelist, gchar **blacklist)
{
    GError *error;
    GDir *plugins_dir;
    const gchar *file;


#ifdef DEBUG
    g_debug("Scanning notification backends dir: %s", backends_dir_name);
#endif /* DEBUG */

    plugins_dir = g_dir_open(backends_dir_name, 0, &error);
    if ( ! plugins_dir )
    {
        g_warning("Couldn't read the backends directory: %s", error->message);
        g_clear_error(&error);
        return;
    }

    while ( ( file = g_dir_read_name(plugins_dir) ) != NULL )
    {
        gchar *full_filename;
        EventdNdBackend *backend;
        const gchar **id;
        EventdNdBackendGetInfoFunc get_info;
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

        full_filename = g_build_filename(backends_dir_name, file, NULL);

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

        if ( ! g_module_symbol(module, "eventd_nd_backend_id", (void **)&id) )
            continue;

        if ( id == NULL )
        {
            g_warning("Backend '%s' must define eventd_nd_backend_id", file);
            continue;
        }

        if ( g_hash_table_contains(backends, *id) )
        {
#ifdef DEBUG
            g_debug("Backend '%s' with id '%s' already loaded", file, *id);
#endif /* ! DEBUG */
            continue;
        }

        if ( ! g_module_symbol(module, "eventd_nd_backend_get_info", (void **)&get_info) )
            continue;

#ifdef DEBUG
        g_debug("Loading backend '%s': %s", file, *id);
#endif /* ! DEBUG */

        backend = g_new0(EventdNdBackend, 1);
        get_info(backend);
        backend->module = module;
        backend->id = g_strdup(*id);
        backend->context = backend->init(context, interface);

        g_hash_table_insert(backends, backend->id, backend);
    }
    g_dir_close(plugins_dir);
}

GHashTable *
eventd_nd_backends_load(EventdNdContext *context, EventdNdInterface *interface)
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
        return NULL;
    }

    GHashTable *backends;

    backends = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, _eventd_nd_backend_free);

    env_whitelist = g_getenv("EVENTD_NOTIFICATION_BACKENDS_WHITELIST");
    if ( env_whitelist != NULL )
        whitelist = g_strsplit(env_whitelist, ",", 0);

    env_blacklist = g_getenv("EVENTD_NOTIFICATION_BACKENDS_BLACKLIST");
    if ( env_blacklist != NULL )
        blacklist = g_strsplit(env_blacklist, ",", 0);

    env_base_dir = g_getenv("EVENTD_NOTIFICATION_BACKENDS_DIR");
    if ( env_base_dir != NULL )
    {
        if ( env_base_dir[0] == '~' )
            plugins_dir = g_build_filename(g_get_home_dir(), env_base_dir+2, NULL);
        else
            plugins_dir = g_build_filename(env_base_dir,  NULL);

        if ( g_file_test(plugins_dir, G_FILE_TEST_IS_DIR) )
            _eventd_nd_backends_load_dir(context, interface, backends, plugins_dir, whitelist, blacklist);
        g_free(plugins_dir);
    }

    plugins_dir = g_build_filename(g_get_user_data_dir(), PACKAGE_NAME, "plugins", "nd", NULL);
    if ( g_file_test(plugins_dir, G_FILE_TEST_IS_DIR) )
        _eventd_nd_backends_load_dir(context, interface, backends, plugins_dir, whitelist, blacklist);
    g_free(plugins_dir);

    plugins_dir = g_build_filename(DATADIR, PACKAGE_NAME, "plugins", "nd", NULL);
    if ( g_file_test(plugins_dir, G_FILE_TEST_IS_DIR) )
        _eventd_nd_backends_load_dir(context, interface, backends, plugins_dir, whitelist, blacklist);
    g_free(plugins_dir);

    plugins_dir = g_build_filename(LIBDIR, PACKAGE_NAME, "plugins", "nd", NULL);
    if ( g_file_test(plugins_dir, G_FILE_TEST_IS_DIR) )
        _eventd_nd_backends_load_dir(context, interface, backends, plugins_dir, whitelist, blacklist);
    g_free(plugins_dir);

    return backends;
}
