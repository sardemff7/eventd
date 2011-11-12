/*
 * eventd - Small daemon to act on remote or local events
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

#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <signal.h>
#include <errno.h>

#include <glib.h>
#include <gio/gio.h>

#define CONFIG_RELOAD_DELAY 10

#include <libeventd-client.h>
#include <libeventd-event.h>
#include <eventd-plugin.h>
#include "plugins.h"
#include "config.h"


GHashTable *config = NULL;

static void
eventd_config_parse_server(GKeyFile *config_file)
{
    GError *error = NULL;
    gchar **keys = NULL;
    gchar **key = NULL;

    if ( g_key_file_has_group(config_file, "server") )
    {
        keys = g_key_file_get_keys(config_file, "server", NULL, &error);
        for ( key = keys ; *key ; ++key )
            g_hash_table_insert(config, g_strdup(*key), g_key_file_get_value(config_file, "server", *key, NULL));
        g_strfreev(keys);
    }
}

static void
eventd_config_parse_client(gchar *type, gchar *config_dir_name)
{
    GError *error = NULL;
    GDir *config_dir = NULL;
    gchar *file = NULL;
    gchar *config_file_name = NULL;
    GKeyFile *config_file = NULL;

    config_file_name = g_strconcat(config_dir_name, ".conf", NULL);
    if ( g_file_test(config_file_name, G_FILE_TEST_IS_REGULAR) )
    {
        config_file = g_key_file_new();
        if ( ! g_key_file_load_from_file(config_file, config_file_name, G_KEY_FILE_NONE, &error) )
            g_warning("Can't read the defaults file '%s': %s", config_file_name, error->message);
        else
            eventd_plugins_event_parse_all(type, NULL, config_file);
        g_clear_error(&error);
        g_key_file_free(config_file);
    }
    g_free(config_file_name);

    config_dir = g_dir_open(config_dir_name, 0, &error);
    if ( ! config_dir )
    {
        g_warning("Can't read the configuration directory '%s': %s", config_dir_name, error->message);
        g_clear_error(&error);
        return;
    }

    while ( ( file = (gchar *)g_dir_read_name(config_dir) ) != NULL )
    {
        gchar *event = NULL;

        if ( g_str_has_prefix(file, ".") || ( ! g_str_has_suffix(file, ".conf") ) )
            continue;

        event = g_strndup(file, strlen(file) - 5);

        #if DEBUG
        g_debug("Parsing event '%s' of client type '%s'", event, type);
        #endif /* DEBUG */

        config_file_name = g_build_filename(config_dir_name, file, NULL);
        config_file = g_key_file_new();
        if ( ! g_key_file_load_from_file(config_file, config_file_name, G_KEY_FILE_NONE, &error) )
            goto next;

        eventd_plugins_event_parse_all(type, event, config_file);

    next:
        g_free(event);
        if ( error )
            g_warning("Can't read the configuration file '%s': %s", config_file_name, error->message);
        g_clear_error(&error);
        g_key_file_free(config_file);
        g_free(config_file_name);
    }

    g_dir_close(config_dir);
}

void
eventd_config_load_dir(const gchar *base_dir)
{
    GError *error = NULL;
    gchar *config_dir_name = NULL;
    GDir *config_dir = NULL;
    gchar *client_config_dir_name = NULL;
    gchar *file = NULL;
    gchar *config_file_name = NULL;
    GKeyFile *config_file = NULL;


    config_dir_name = g_build_filename(base_dir, PACKAGE_NAME, NULL);

    if ( ! g_file_test(config_dir_name, G_FILE_TEST_IS_DIR) )
        goto out;

    config_file_name = g_build_filename(config_dir_name, PACKAGE_NAME ".conf", NULL);
    if ( g_file_test(config_file_name, G_FILE_TEST_IS_REGULAR) )
    {
        config_file = g_key_file_new();
        if ( ! g_key_file_load_from_file(config_file, config_file_name, G_KEY_FILE_NONE, &error) )
            g_warning("Can't read the configuration file '%s': %s", config_file_name, error->message);
        else
            eventd_config_parse_server(config_file);
        g_clear_error(&error);
        g_key_file_free(config_file);
    }
    g_free(config_file_name);

    eventd_plugins_config_init_all();

    config_dir = g_dir_open(config_dir_name, 0, &error);
    if ( ! config_dir )
        goto out;

    while ( ( file = (gchar *)g_dir_read_name(config_dir) ) != NULL )
    {
        if ( g_str_has_prefix(file, ".") )
            continue;

        client_config_dir_name = g_build_filename(config_dir_name, file, NULL);

        if ( g_file_test(client_config_dir_name, G_FILE_TEST_IS_DIR) )
            eventd_config_parse_client(file, client_config_dir_name);

        g_free(client_config_dir_name);
    }
    g_dir_close(config_dir);

out:
    if ( error )
        g_warning("Can't read the configuration directory: %s", error->message);
    g_clear_error(&error);
    g_free(config_dir_name);
}

void
eventd_config_parser()
{
    if ( config )
    {
        g_message("Reloading configuration");
        eventd_config_clean();
    }

    config = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    eventd_config_load_dir(DATADIR);
    eventd_config_load_dir(SYSCONFDIR);
    eventd_config_load_dir(g_get_user_config_dir());
}

void
eventd_config_clean()
{
    eventd_plugins_config_clean_all();

    g_hash_table_remove_all(config);
}


guint64
eventd_config_get_guint64(const gchar *name, guint64 default_value)
{
    gchar *value = NULL;

    value = g_hash_table_lookup(config, name);

    if ( value == NULL )
        return default_value;
    else
        return g_ascii_strtoull(value, NULL, 10);
}

gint64
eventd_config_get_gint64(const gchar *name, gint64 default_value)
{
    gchar *value = NULL;

    value = g_hash_table_lookup(config, name);

    if ( value == NULL )
        return default_value;
    else
        return g_ascii_strtoll(value, NULL, 10);
}
