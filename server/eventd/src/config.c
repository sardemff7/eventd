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

#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <libeventd-event.h>
#include <libeventd-config.h>

#include "types.h"

#include "plugins.h"

#include "config.h"

struct _EventdConfig {
    gboolean loaded;
    guint64 stack;
    gint64 timeout;
    GHashTable *event_ids;
    GHashTable *events;
};

typedef struct {
    gint64 timeout;
} EventdConfigEvent;

static gchar *
_eventd_config_events_get_name(const gchar *category, const gchar *name)
{
    return ( name == NULL ) ? g_strdup(category) : g_strconcat(category, "-", name, NULL);
}

const gchar *
eventd_config_get_event_config_id(EventdConfig *config, EventdEvent *event)
{
    const gchar *category;
    gchar *name;
    const gchar *id;

    category = eventd_event_get_category(event);

    name = _eventd_config_events_get_name(category, eventd_event_get_name(event));

    id = g_hash_table_lookup(config->event_ids, name);
    g_free(name);
    if ( id == NULL )
        id = g_hash_table_lookup(config->event_ids, category);

    return id;
}

static void
_eventd_config_event_free(gpointer data)
{
    EventdConfigEvent *event = data;

    g_free(event);
}

gint64
eventd_config_event_get_timeout(EventdConfig *config, const gchar *config_id)
{
    EventdConfigEvent *config_event;

    config_event = g_hash_table_lookup(config->events, config_id);

    if ( ( config_event == NULL ) || ( config_event->timeout < 0 ) )
        return config->timeout;
    else
        return config_event->timeout;
}


static void
_eventd_config_defaults(EventdConfig *config)
{
    config->stack = 1;
    config->timeout = 3000;
}

static void
_eventd_config_parse_global(EventdConfig *config, GKeyFile *config_file)
{
    Int integer;

    if ( ! g_key_file_has_group(config_file, "Event") )
        return;

    if ( libeventd_config_key_file_get_int(config_file, "Event", "Stack", &integer) == 0 )
        config->stack = integer.value;

    if ( libeventd_config_key_file_get_int(config_file, "Event", "Timeout", &integer) == 0 )
        config->timeout = integer.value;
}

static void
_eventd_config_parse_client(EventdConfig *config, const gchar *id, GKeyFile *config_file)
{
    EventdConfigEvent *event;
    gboolean disable;
    Int timeout;

    if ( ( libeventd_config_key_file_get_boolean(config_file, "Event", "Disable", &disable) == 0 ) && disable )
    {
        g_hash_table_insert(config->events, g_strdup(id), NULL);
        return;
    }

    event = g_new0(EventdConfigEvent, 1);

    event->timeout = -1;


    if ( libeventd_config_key_file_get_int(config_file, "Event", "Timeout", &timeout) == 0 )
        event->timeout = timeout.value;

    g_hash_table_insert(config->events, g_strdup(id), event);
}

static void
_eventd_config_parse_event_file(EventdConfig *config, const gchar *id, GKeyFile *config_file)
{
    gchar *category = NULL;
    gchar *name = NULL;

#if DEBUG
    g_debug("Parsing event '%s'", id);
#endif /* DEBUG */

    if ( libeventd_config_key_file_get_string(config_file, "Event", "Category", &category) != 0 )
        return;

    if ( libeventd_config_key_file_get_string(config_file, "Event", "Name", &name) < 0 )
        goto fail;

    gchar *internal_name;

    internal_name = _eventd_config_events_get_name(category, name);

    g_hash_table_insert(config->event_ids, internal_name, g_strdup(id));

    _eventd_config_parse_client(config, id, config_file);
    eventd_plugins_event_parse_all(id, config_file);

fail:
    g_free(name);
    g_free(category);
}

static void
_eventd_config_read_dir(EventdConfig *config, GHashTable *config_files, const gchar *config_dir_name)
{
    GError *error = NULL;
    GDir *config_dir = NULL;

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
            else if ( ! g_key_file_has_group(config_file, "Event") )
                g_key_file_free(config_file);
            else
                g_hash_table_insert(config_files, g_strdup(file), config_file);
        }
        else if ( g_file_test(config_file_name, G_FILE_TEST_IS_DIR) )
            _eventd_config_read_dir(config, config_files, config_file_name);

        g_free(config_file_name);
    }
}

static void
_eventd_config_load_dir(EventdConfig *config, GHashTable *config_files, const gchar *config_dir_name)
{
    GError *error = NULL;
    gchar *config_file_name = NULL;
    GKeyFile *config_file = NULL;

    if ( ! g_file_test(config_dir_name, G_FILE_TEST_IS_DIR) )
        return;

    config_file_name = g_build_filename(config_dir_name, PACKAGE_NAME ".conf", NULL);
    if ( g_file_test(config_file_name, G_FILE_TEST_IS_REGULAR) )
    {
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

    _eventd_config_read_dir(config, config_files, config_dir_name);
}

static GKeyFile *
_eventd_config_process_config_file(GHashTable *config_files, const gchar *id, GKeyFile *config_file)
{
    gchar *parent_id;

    switch ( libeventd_config_key_file_get_string(config_file, "Event", "Extends", &parent_id) )
    {
    case 1:
        return config_file;
    case -1:
        return NULL;
    case 0:
    break;
    }

    GError *error = NULL;
    if ( ! g_key_file_remove_key(config_file, "Event", "Extends", &error) )
    {
        g_warning("Couldn't clean event file '%s': %s", id, error->message);
        g_clear_error(&error);
        goto fail;
    }

    GKeyFile *parent;
    parent = g_hash_table_lookup(config_files, parent_id);
    if ( parent == NULL )
    {
        g_warning("Event file '%s' has no parent file '%s'", id, parent_id);
        goto fail;
    }

    if ( ( parent = _eventd_config_process_config_file(config_files, parent_id, parent) ) == NULL )
        goto fail;

    GString *merged_data;
    gchar *data;

    data = g_key_file_to_data(parent, NULL, NULL);
    merged_data = g_string_new(data);
    g_free(data);

    data = g_key_file_to_data(config_file, NULL, NULL);
    g_string_append(merged_data, data);
    g_free(data);

    GKeyFile *new_config_file;

    new_config_file = g_key_file_new();
    if ( g_key_file_load_from_data(new_config_file, merged_data->str, -1, G_KEY_FILE_NONE, &error) )
        g_hash_table_insert(config_files, g_strdup(id), new_config_file);
    else
    {
        g_warning("Couldn't merge '%s' and '%s': %s", id, parent_id, error->message);
        g_clear_error(&error);
        g_key_file_free(new_config_file);
        new_config_file = NULL;
    }

    g_string_free(merged_data, TRUE);
    g_free(parent_id);

    return new_config_file;

fail:
    g_free(parent_id);
    return NULL;
}

EventdConfig *
eventd_config_new()
{
    EventdConfig *config;

    config = g_new0(EventdConfig, 1);

    config->event_ids = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    config->events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, _eventd_config_event_free);

    return config;
}

static void
_eventd_config_clean(EventdConfig *config)
{
    if ( ! config->loaded )
        return;

    eventd_plugins_config_reset_all();

    g_hash_table_remove_all(config->events);
    g_hash_table_remove_all(config->event_ids);
}

void
eventd_config_parse(EventdConfig *config)
{
    _eventd_config_clean(config);

    config->loaded = TRUE;

    _eventd_config_defaults(config);

    GHashTable *config_files;

    config_files = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_key_file_free);

    _eventd_config_load_dir(config, config_files, DATADIR G_DIR_SEPARATOR_S PACKAGE_NAME);
    _eventd_config_load_dir(config, config_files, SYSCONFDIR G_DIR_SEPARATOR_S PACKAGE_NAME);

    gchar *user_config_dir;
    user_config_dir = g_build_filename(g_get_user_config_dir(), PACKAGE_NAME, NULL);
    _eventd_config_load_dir(config, config_files, user_config_dir);
    g_free(user_config_dir);

    const gchar *env_config_dir;
    env_config_dir = g_getenv("EVENTD_CONFIG_DIR");
    if ( env_config_dir != NULL )
        _eventd_config_load_dir(config, config_files, env_config_dir);

    GHashTableIter iter;
    gchar *id;
    GKeyFile *config_file;
    g_hash_table_iter_init(&iter, config_files);
    while ( g_hash_table_iter_next(&iter, (gpointer *)&id, (gpointer *)&config_file) )
    {
        if ( ( config_file = _eventd_config_process_config_file(config_files, id, config_file) ) != NULL )
            _eventd_config_parse_event_file(config, id, config_file);
    }
    g_hash_table_unref(config_files);
}

void
eventd_config_free(EventdConfig *config)
{
    _eventd_config_clean(config);

    g_hash_table_unref(config->events);
    g_hash_table_unref(config->event_ids);

    g_free(config);
}

guint64
eventd_config_get_stack(EventdConfig *config)
{
    return config->stack;
}
