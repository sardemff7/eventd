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
    gint64 max_clients;
    gchar *avahi_name;
    GHashTable *events;
};

typedef struct {
    gboolean disable;
    gint64 timeout;
} EventdConfigEvent;

static void
_eventd_config_event_update(EventdConfigEvent *event, gboolean disable, Int *timeout)
{
    event->disable = disable;
    if ( timeout->set )
        event->timeout = timeout->value;
}

static EventdConfigEvent *
_eventd_config_event_new(gboolean disable, Int *timeout, EventdConfigEvent *parent)
{
    EventdConfigEvent *event;

    timeout->value = timeout->set ? timeout->value : ( parent != NULL ) ? parent->timeout : -1;
    timeout->set = TRUE;

    event = g_new0(EventdConfigEvent, 1);

    _eventd_config_event_update(event, disable, timeout);

    return event;
}

static void
_eventd_config_event_free(gpointer data)
{
    EventdConfigEvent *event = data;

    g_free(event);
}

void
eventd_config_event_get_disable_and_timeout(EventdConfig *config, EventdEvent *event, gboolean *disable, gint64 *timeout)
{
    EventdConfigEvent *config_event;

    config_event = libeventd_config_events_get_event(config->events, eventd_event_get_category(event), eventd_event_get_name(event));

    if ( config_event == NULL )
    {
        *disable = FALSE;
        *timeout = -1;
    }
    else
    {
        *disable = config_event->disable;
        *timeout = config_event->timeout;
    }
}


static void
_eventd_config_defaults(EventdConfig *config)
{
    config->max_clients = -1;
    config->avahi_name = g_strdup(PACKAGE_NAME);
}

static void
_eventd_config_parse_server(EventdConfig *config, GKeyFile *config_file)
{
    Int integer;
    gchar *avahi_name;

    if ( ! g_key_file_has_group(config_file, "server") )
        return;

    if ( libeventd_config_key_file_get_int(config_file, "server", "max-clients", &integer) < 0 )
        return;
    if ( integer.set )
        config->max_clients = integer.value;

    if ( libeventd_config_key_file_get_string(config_file, "server", "avahi-name", &avahi_name) < 0 )
        return;
    if ( avahi_name != NULL )
    {
        g_free(config->avahi_name);
        config->avahi_name= avahi_name;
    }
}

static void
_eventd_config_parse_client(EventdConfig *config, const gchar *event_category, const gchar *event_name, GKeyFile *config_file)
{
    EventdConfigEvent *event;
    gchar *name;
    gboolean disable;
    Int timeout;

    if ( ! g_key_file_has_group(config_file, "event") )
        return;

    if ( libeventd_config_key_file_get_boolean(config_file, "event", "disable", &disable) < 0 )
        goto skip;

    if ( libeventd_config_key_file_get_int(config_file, "event", "timeout", &timeout) < 0 )
        goto skip;

    name = libeventd_config_events_get_name(event_category, event_name);

    event = g_hash_table_lookup(config->events, name);
    if ( event != NULL )
        _eventd_config_event_update(event, disable, &timeout);
    else
        g_hash_table_insert(config->events, name, _eventd_config_event_new(disable, &timeout, g_hash_table_lookup(config->events, event_category)));

skip:
    {}
}

static void
_eventd_config_parse_client_dir(EventdConfig *config, const gchar *type, gchar *config_dir_name)
{
    GError *error = NULL;
    GDir *config_dir = NULL;
    const gchar *file = NULL;
    gchar *config_file_name = NULL;
    GKeyFile *config_file = NULL;

    config_dir = g_dir_open(config_dir_name, 0, &error);
    if ( ! config_dir )
    {
        g_warning("Can't read the configuration directory '%s': %s", config_dir_name, error->message);
        g_clear_error(&error);
        return;
    }

    while ( ( file = g_dir_read_name(config_dir) ) != NULL )
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

        _eventd_config_parse_client(config, type, event, config_file);
        eventd_plugins_event_parse_all(type, event, config_file);

    next:
        g_free(event);
        if ( error != NULL )
            g_warning("Can't read the configuration file '%s': %s", config_file_name, error->message);
        g_clear_error(&error);
        g_key_file_free(config_file);
        g_free(config_file_name);
    }

    g_dir_close(config_dir);
}

static void
_eventd_config_load_dir(EventdConfig *config, const gchar *base_dir)
{
    GError *error = NULL;
    gchar *config_dir_name = NULL;
    GDir *config_dir = NULL;
    const gchar *file = NULL;
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
            _eventd_config_parse_server(config, config_file);
        g_clear_error(&error);
        g_key_file_free(config_file);
    }
    g_free(config_file_name);

    config_dir = g_dir_open(config_dir_name, 0, &error);
    if ( ! config_dir )
        goto out;

    while ( ( file = g_dir_read_name(config_dir) ) != NULL )
    {
        if ( g_str_has_prefix(file, ".") || ( ! g_str_has_suffix(file, ".conf") ) )
            continue;

        config_file_name = g_build_filename(config_dir_name, file, NULL);

        if ( g_file_test(config_file_name, G_FILE_TEST_IS_REGULAR) )
        {
            gchar *type;

            type = g_strndup(file, strlen(file) - 5);
            config_file = g_key_file_new();
#if DEBUG
            g_debug("Parsing event defaults of client type '%s'", type);
#endif /* DEBUG */
            if ( ! g_key_file_load_from_file(config_file, config_file_name, G_KEY_FILE_NONE, &error) )
                g_warning("Can't read the defaults file '%s': %s", config_file_name, error->message);
            else
                eventd_plugins_event_parse_all(type, NULL, config_file);
            g_clear_error(&error);
            g_key_file_free(config_file);
            g_free(type);
        }

        g_free(config_file_name);
    }

    g_dir_rewind(config_dir);
    while ( ( file = g_dir_read_name(config_dir) ) != NULL )
    {
        if ( g_str_has_prefix(file, ".") )
            continue;

        config_file_name = g_build_filename(config_dir_name, file, NULL);

        if ( g_file_test(config_file_name, G_FILE_TEST_IS_DIR) )
            _eventd_config_parse_client_dir(config, file, config_file_name);

        g_free(config_file_name);
    }
    g_dir_close(config_dir);

out:
    if ( error != NULL )
        g_warning("Can't read the configuration directory: %s", error->message);
    g_clear_error(&error);
    g_free(config_dir_name);
}

EventdConfig *
eventd_config_parser(EventdConfig *config)
{
    if ( config != NULL )
    {
        g_message("Reloading configuration");
        eventd_config_clean(config);
    }

    config = g_new0(EventdConfig, 1);

    config->events = libeventd_config_events_new(_eventd_config_event_free);

    _eventd_config_defaults(config);

    eventd_plugins_config_init_all();

    _eventd_config_load_dir(config, DATADIR);
    _eventd_config_load_dir(config, SYSCONFDIR);
    _eventd_config_load_dir(config, g_get_user_config_dir());

    return config;
}

void
eventd_config_clean(EventdConfig *config)
{
    eventd_plugins_config_clean_all();

    g_free(config);
}

gint64
eventd_config_get_max_clients(EventdConfig *config)
{
    return config->max_clients;
}

const gchar *
eventd_config_get_avahi_name(EventdConfig *config)
{
    return config->avahi_name;
}
