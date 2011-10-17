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

#define DEFAULT_DELAY 5
#define DEFAULT_DELAY_STR "5"

#include <eventd-plugin.h>
#include "plugins.h"
#include "events.h"


GHashTable *config = NULL;

static void
eventd_parse_server(GKeyFile *config_file)
{
    GError *error = NULL;
    gchar **keys = NULL;
    gchar **key = NULL;

    config = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    if ( g_key_file_has_group(config_file, "server") )
    {
        keys = g_key_file_get_keys(config_file, "server", NULL, &error);
        for ( key = keys ; *key ; ++key )
            g_hash_table_insert(config, g_strdup(*key), g_key_file_get_value(config_file, "server", *key, NULL));
        g_strfreev(keys);
    }
}

static void
eventd_init_default_server_config()
{
    if ( ! config )
        config = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    if ( g_hash_table_lookup(config, "delay") == NULL )
        g_hash_table_insert(config, g_strdup("delay"), g_strdup(DEFAULT_DELAY_STR));
}

gint8
eventd_config_key_file_get_string(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *event, const gchar *type, gchar **value)
{
    GError *error = NULL;
    gint8 ret = 0;
    gchar *ret_value = NULL;

    ret_value = g_key_file_get_string(config_file, group, key, &error);
    if ( ( ! ret_value ) && ( error->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND ) )
    {
        ret = -1;
        g_warning("Can't set the %s action of event '%s' for client type '%s': %s", group, event, type, error->message);
    }
    else if ( ! ret_value )
    {
        ret = 1;
        *value = NULL;
    }
    else
        *value = ret_value;
    g_clear_error(&error);

    return ret;
}

typedef struct {
    gchar *type;
    gchar *event;
    GKeyFile *config_file;
    GKeyFile *defaults_config_file;
} EventdConfigParseData;

static void
eventd_plugin_event_parse(EventdPlugin *plugin, EventdConfigParseData *data)
{
    plugin->event_parse(data->type, data->event, data->config_file, data->defaults_config_file);
}

static void
eventd_parse_client(gchar *type, gchar *config_dir_name)
{
    GError *error = NULL;
    GDir *config_dir = NULL;
    gchar *file = NULL;
    gchar *defaults_config_file_name = NULL;

    EventdConfigParseData data = {
        .type = type,
        .event = NULL,
        .config_file = NULL,
        .defaults_config_file = NULL
    };

    defaults_config_file_name = g_strdup_printf("%s.conf", config_dir_name);
    if ( g_file_test(defaults_config_file_name, G_FILE_TEST_IS_REGULAR) )
    {
        data.defaults_config_file = g_key_file_new();
        if ( ! g_key_file_load_from_file(data.defaults_config_file, defaults_config_file_name, G_KEY_FILE_NONE, &error) )
        {
            g_warning("Can't read the defaults file '%s': %s", defaults_config_file_name, error->message);
            g_clear_error(&error);
            g_key_file_free(data.defaults_config_file);
            data.defaults_config_file = NULL;
        }
    }
    g_free(defaults_config_file_name);

    config_dir = g_dir_open(config_dir_name, 0, &error);
    if ( ! config_dir )
    {
        g_warning("Can't read the configuration directory '%s': %s", config_dir_name, error->message);
        g_clear_error(&error);
        return;
    }

    while ( ( file = (gchar *)g_dir_read_name(config_dir) ) != NULL )
    {
        gchar *config_file_name = NULL;

        if ( g_str_has_prefix(file, ".") || ( ! g_str_has_suffix(file, ".conf") ) )
            continue;

        data.event = g_strndup(file, strlen(file) - 5);

        #if DEBUG
        g_debug("Parsing event '%s' of client type '%s'", data.event, type);
        #endif /* DEBUG */

        config_file_name = g_strdup_printf("%s/%s", config_dir_name, file);
        data.config_file = g_key_file_new();
        if ( ! g_key_file_load_from_file(data.config_file, config_file_name, G_KEY_FILE_NONE, &error) )
            goto next;

        eventd_plugins_foreach((GFunc)eventd_plugin_event_parse, &data);

    next:
        g_free(data.event);
        data.event = NULL;
        if ( error )
            g_warning("Can't read the configuration file '%s': %s", config_file_name, error->message);
        g_clear_error(&error);
        g_key_file_free(data.config_file);
        g_free(config_file_name);
    }

    g_dir_close(config_dir);
    if ( data.defaults_config_file )
        g_key_file_free(data.defaults_config_file);
}

static void
eventd_plugin_config_init(EventdPlugin *plugin, gpointer data)
{
    plugin->config_init();
}

void
eventd_config_parser()
{
    GError *error = NULL;
    gchar *config_dir_name = NULL;
    GDir *config_dir = NULL;
    gchar *client_config_dir_name = NULL;
    gchar *file = NULL;
    gchar *config_file_name = NULL;
    GKeyFile *config_file = NULL;


    config_dir_name = g_strdup_printf("%s/"PACKAGE_NAME, g_get_user_config_dir());


    if ( config )
    {
        g_message("Reloading configuration");
        eventd_config_clean();
    }
    else
    {
        /*
         * We monitor the various dirs we use to
         * reload the configuration if the user
         * change it
         */
        GFile *dir = NULL;
        GFileMonitor *monitor = NULL;

        g_message("First configuration load");

        dir = g_file_new_for_path(config_dir_name);
        if ( ( monitor = g_file_monitor(dir, G_FILE_MONITOR_NONE, NULL, &error) ) == NULL )
            g_warning("Couldn't monitor the main config directory: %s", error->message);
        else
            g_signal_connect(monitor, "changed", eventd_config_parser, NULL);
        g_clear_error(&error);
        g_object_unref(dir);
    }

    config_file_name = g_strdup_printf("%s/"PACKAGE_NAME".conf", config_dir_name);
    config_file = g_key_file_new();
    if ( ! g_key_file_load_from_file(config_file, config_file_name, G_KEY_FILE_NONE, &error) )
        g_warning("Can't read the configuration file '%s': %s", config_file_name, error->message);
    else
        eventd_parse_server(config_file);
    g_clear_error(&error);
    g_key_file_free(config_file);
    g_free(config_file_name);

    eventd_plugins_foreach((GFunc)eventd_plugin_config_init, NULL);

    config_dir = g_dir_open(config_dir_name, 0, &error);
    if ( ! config_dir )
        goto out;

    while ( ( file = (gchar *)g_dir_read_name(config_dir) ) != NULL )
    {
        if ( g_str_has_prefix(file, ".") )
            continue;

        client_config_dir_name = g_strdup_printf("%s/%s", config_dir_name, file);

        if ( g_file_test(client_config_dir_name, G_FILE_TEST_IS_DIR) )
        {
            if ( g_hash_table_lookup(config, file) == NULL )
            {
                GFile *dir = NULL;
                GFileMonitor *monitor = NULL;

                dir = g_file_new_for_path(client_config_dir_name);
                if ( ( monitor = g_file_monitor(dir, G_FILE_MONITOR_NONE, NULL, &error) ) == NULL )
                    g_warning("Couldn't monitor the config directory '%s': %s", client_config_dir_name, error->message);
                else
                    g_signal_connect(monitor, "changed", eventd_config_parser, NULL);
                g_clear_error(&error);
                g_object_unref(dir);
            }

            eventd_parse_client(file, client_config_dir_name);
        }

        g_free(client_config_dir_name);
    }
    g_dir_close(config_dir);

    if ( ! config )
        eventd_init_default_server_config();
out:
    if ( error )
        g_warning("Can't read the configuration directory: %s", error->message);
    g_clear_error(&error);
    g_free(config_dir_name);
}

static void
eventd_plugin_config_clean(EventdPlugin *plugin, gpointer data)
{
    plugin->config_clean();
}

void
eventd_config_clean()
{
    eventd_plugins_foreach((GFunc)eventd_plugin_config_clean, NULL);

    g_hash_table_remove_all(config);
}


guint64
eventd_config_get_guint64(const gchar *name)
{
    gchar *value = NULL;

    value = g_hash_table_lookup(config, name);

    return g_ascii_strtoull(value, NULL, 10);
}
