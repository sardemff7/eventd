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

#include <config.h>

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <libeventd-event.h>
#include <libeventd-helpers-config.h>
#include <libeventd-helpers-dirs.h>
#include <eventdctl.h>

#include "types.h"

#include "plugins.h"
#include "actions.h"
#include "events.h"

#include "config.h"

struct _EventdConfig {
    gboolean loaded;
    const gchar *gnutls_priorities_env;
    gchar *gnutls_priorities;
    EventdEvents *events;
    EventdActions *actions;
};

gboolean
eventd_config_process_event(EventdConfig *config, EventdEvent *event, GQuark *flags, const GList **actions)
{
    return eventd_events_process_event(config->events, event, flags, actions);
}


static void
_eventd_config_defaults(EventdConfig *config)
{
}

static void
_eventd_config_parse_global(EventdConfig *config, GKeyFile *config_file)
{
    if ( g_tls_backend_supports_tls(g_tls_backend_get_default()) && g_key_file_has_group(config_file, "Server") )
    {
        gchar *priorities;
        if ( ( config->gnutls_priorities_env == NULL ) && ( evhelpers_config_key_file_get_string(config_file, "Server", "GnuTLSPriority", &priorities) == 0 ) )
        {
            g_free(config->gnutls_priorities);
            config->gnutls_priorities = priorities;
        }

    }
}

static void
_eventd_config_read_dir(EventdConfig *config, GHashTable *action_files, GHashTable *event_files, const gchar *config_dir_name)
{
    GError *error = NULL;
    GDir *config_dir;

    config_dir = g_dir_open(config_dir_name, 0, &error);
    if ( config_dir == NULL )
    {
        g_warning("Can't read configuration directory '%s': %s", config_dir_name, error->message);
        g_clear_error(&error);
        return;
    }

    gchar *config_file_name = NULL;
    GKeyFile *config_file = NULL;
    const gchar *file;
    while ( ( file = g_dir_read_name(config_dir) ) != NULL )
    {
        if ( g_str_has_prefix(file, ".") )
            continue;

        config_file_name = g_build_filename(config_dir_name, file, NULL);

        if ( g_str_has_suffix(file, ".event") && g_file_test(config_file_name, G_FILE_TEST_IS_REGULAR) )
        {
            config_file = g_key_file_new();
            if ( ! g_key_file_load_from_file(config_file, config_file_name, G_KEY_FILE_NONE, &error) )
            {
                g_warning("Can't read the defaults file '%s': %s", config_file_name, error->message);
                g_clear_error(&error);
                g_key_file_free(config_file);
            }
            else
                g_hash_table_insert(event_files, g_strdup(file), config_file);
        }
        else if ( g_str_has_suffix(file, ".action") && g_file_test(config_file_name, G_FILE_TEST_IS_REGULAR) )
        {
            config_file = g_key_file_new();
            if ( ! g_key_file_load_from_file(config_file, config_file_name, G_KEY_FILE_NONE, &error) )
            {
                g_warning("Can't read the defaults file '%s': %s", config_file_name, error->message);
                g_clear_error(&error);
                g_key_file_free(config_file);
            }
            else if ( ! g_key_file_has_group(config_file, "Action") )
                g_key_file_free(config_file);
            else
                g_hash_table_insert(action_files, g_strdup(file), config_file);
        }
        else if ( g_file_test(config_file_name, G_FILE_TEST_IS_DIR) )
            _eventd_config_read_dir(config, action_files, event_files, config_file_name);

        g_free(config_file_name);
    }
    g_dir_close(config_dir);
}

static void
_eventd_config_load_dir(EventdConfig *config, GHashTable *action_files, GHashTable *event_files, gchar *config_dir_name)
{
    GError *error = NULL;
    gchar *config_file_name = NULL;

#ifdef EVENTD_DEBUG
    g_debug("Scanning configuration dir: %s", config_dir_name);
#endif /* EVENTD_DEBUG */

    config_file_name = g_build_filename(config_dir_name, PACKAGE_NAME ".conf", NULL);
    if ( g_file_test(config_file_name, G_FILE_TEST_IS_REGULAR) )
    {
        GKeyFile *config_file;
        config_file = g_key_file_new();
        if ( ! g_key_file_load_from_file(config_file, config_file_name, G_KEY_FILE_NONE, &error) )
            g_warning("Can't read the configuration file '%s': %s", config_file_name, error->message);
        else
        {
            _eventd_config_parse_global(config, config_file);
            eventd_plugins_global_parse_all(config_file);
        }
        g_clear_error(&error);
        g_key_file_free(config_file);
    }
    g_free(config_file_name);

    _eventd_config_read_dir(config, action_files, event_files, config_dir_name);
    g_free(config_dir_name);
}

static GKeyFile *
_eventd_config_process_config_file(GHashTable *files, const gchar *id, GKeyFile *file)
{
    if ( ! g_key_file_has_group(file, "File") )
        return file;

    gchar *parent_id;

    switch ( evhelpers_config_key_file_get_string(file, "File", "Extends", &parent_id) )
    {
    case 1:
        return file;
    case -1:
        return NULL;
    case 0:
    break;
    }

    GKeyFile *new_file = NULL;

    GError *error = NULL;
    if ( ! g_key_file_remove_key(file, "File", "Extends", &error) )
    {
        g_warning("Couldn't clean event file '%s': %s", id, error->message);
        g_clear_error(&error);
        goto fail;
    }

    GKeyFile *parent;
    parent = g_hash_table_lookup(files, parent_id);
    if ( parent == NULL )
    {
        g_warning("Event file '%s' has no parent file '%s'", id, parent_id);
        goto fail;
    }

    if ( ( parent = _eventd_config_process_config_file(files, parent_id, parent) ) == NULL )
        goto fail;

    GString *merged_data;
    gchar *data;

    data = g_key_file_to_data(parent, NULL, NULL);
    merged_data = g_string_new(data);
    g_free(data);

    data = g_key_file_to_data(file, NULL, NULL);
    g_string_append(merged_data, data);
    g_free(data);

    new_file = g_key_file_new();
    if ( g_key_file_load_from_data(new_file, merged_data->str, -1, G_KEY_FILE_NONE, &error) )
        g_hash_table_insert(files, g_strdup(id), new_file);
    else
    {
        g_warning("Couldn't merge '%s' and '%s': %s", id, parent_id, error->message);
        g_clear_error(&error);
        g_key_file_free(new_file);
        new_file = NULL;
    }

    g_string_free(merged_data, TRUE);

fail:
    g_free(parent_id);

    return new_file;
}

EventdConfig *
eventd_config_new(gboolean system_mode)
{
    EventdConfig *config;

    config = g_new0(EventdConfig, 1);

    config->actions = eventd_actions_new();
    config->events = eventd_events_new();

    config->gnutls_priorities_env = g_getenv("G_TLS_GNUTLS_PRIORITY");

    eventd_config_parse(config, system_mode);

    return config;
}

static void
_eventd_config_clean(EventdConfig *config)
{
    if ( ! config->loaded )
        return;

    eventd_plugins_config_reset_all();

    eventd_events_reset(config->events);
    eventd_actions_reset(config->actions);
}

void
eventd_config_parse(EventdConfig *config, gboolean system_mode)
{
    _eventd_config_clean(config);

    config->loaded = TRUE;

    _eventd_config_defaults(config);

    GHashTable *action_files;
    GHashTable *event_files;

    action_files = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_key_file_free);
    event_files = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_key_file_free);

    gchar **dirs, **dir;
    dirs = evhelpers_dirs_get_config("EVENTD_CONFIG_DIR", system_mode, NULL);
    for ( dir = dirs ; *dir != NULL ; ++dir )
        _eventd_config_load_dir(config, action_files, event_files, *dir);
    g_free(dirs);

    /*
     * We check the env early, and skip the configuration if found
     * so here, we can override it safely
     */
    if ( config->gnutls_priorities != NULL )
        g_setenv("G_TLS_GNUTLS_PRIORITY", config->gnutls_priorities, TRUE);

    GHashTableIter iter;
    gchar *id;
    GKeyFile *config_file;

    g_hash_table_iter_init(&iter, event_files);
    while ( g_hash_table_iter_next(&iter, (gpointer *)&id, (gpointer *)&config_file) )
    {
        if ( ( config_file = _eventd_config_process_config_file(event_files, id, config_file) ) != NULL )
            eventd_events_parse(config->events, config_file);
    }
    g_hash_table_unref(event_files);

    g_hash_table_iter_init(&iter, action_files);
    while ( g_hash_table_iter_next(&iter, (gpointer *)&id, (gpointer *)&config_file) )
    {
        if ( ( config_file = _eventd_config_process_config_file(action_files, id, config_file) ) != NULL )
            eventd_actions_parse(config->actions, config_file, id);
    }
    g_hash_table_unref(action_files);

    eventd_actions_link_actions(config->actions);
    eventd_events_link_actions(config->events, config->actions);
}

void
eventd_config_free(EventdConfig *config)
{
    _eventd_config_clean(config);

    eventd_actions_free(config->actions);
    eventd_events_free(config->events);

    g_free(config);
}
