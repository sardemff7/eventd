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

#include <glib.h>
#include <glib-object.h>

#include <libeventd-event.h>
#include <libeventd-config.h>
#include <eventdctl.h>

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
    gchar *data;
    GRegex *regex;
} EventdConfigDataMatch;

typedef struct {
    gint64 importance;
    gchar *id;

    /* Conditions */
    gchar **if_data;
    GList *if_data_matches;
    GQuark *flags_whitelist;
    GQuark *flags_blacklist;
} EventdConfigMatch;

typedef struct {
    gint64 timeout;
} EventdConfigEvent;

static gchar *
_eventd_config_events_get_name(const gchar *category, const gchar *name)
{
    return ( name == NULL ) ? g_strdup(category) : g_strconcat(category, "-", name, NULL);
}

const gchar *
_eventd_config_get_best_match(GList *list, EventdEvent *event, GQuark *current_flags)
{
    GList *match_;
    for ( match_ = list ; match_ != NULL ; match_ = g_list_next(match_) )
    {
        EventdConfigMatch *match = match_->data;
        gboolean skip = FALSE;

        if ( match->if_data != NULL )
        {
            gchar **data;
            for ( data = match->if_data ; ( *data != NULL ) && ( ! skip )  ; ++data )
            {
                if ( ! eventd_event_has_data(event, *data) )
                    skip = TRUE;
            }
            if ( skip )
                continue;
        }

        if ( match->if_data_matches != NULL )
        {
            GList *data_match_;
            const gchar *data;
            for ( data_match_ = match->if_data_matches ; ( data_match_ != NULL ) && ( ! skip ) ; data_match_ = g_list_next(data_match_) )
            {
                EventdConfigDataMatch *data_match = data_match_->data;
                if ( ( data = eventd_event_get_data(event, data_match->data) ) == NULL )
                    continue;
                if ( ! g_regex_match(data_match->regex, data, 0, NULL) )
                    skip = TRUE;
            }
            if ( skip )
                continue;
        }

        if ( current_flags != NULL )
        {
            GQuark *flag;
            if ( match->flags_whitelist != NULL )
            {
                GQuark *wflag;
                for ( wflag = match->flags_whitelist ; ( *wflag != 0 ) && ( ! skip ) ; ++wflag )
                {
                    for ( flag = current_flags ; ( *flag != 0 ) && ( ! skip ) ; ++flag )
                    {
                        if ( *flag != *wflag )
                            skip = TRUE;
                    }
                }
                if ( skip )
                    continue;
            }

            if ( match->flags_blacklist != NULL )
            {
                GQuark *bflag;
                for ( bflag = match->flags_blacklist ; ( *bflag != 0 ) && ( ! skip ) ; ++bflag )
                {
                    for ( flag = current_flags ; ( *flag != 0 ) && ( ! skip ) ; ++flag )
                    {
                        if ( *flag == *bflag )
                            skip = TRUE;
                    }
                }
                if ( skip )
                    continue;
            }
        }

        return match->id;
    }

    return NULL;
}

const gchar *
eventd_config_get_event_config_id(EventdConfig *config, EventdEvent *event, GQuark *current_flags)
{
    const gchar *category;
    GList *list;
    const gchar *match;

    category = eventd_event_get_category(event);

    gchar *name;
    name = _eventd_config_events_get_name(category, eventd_event_get_name(event));
    list = g_hash_table_lookup(config->event_ids, name);
    g_free(name);


    if ( list != NULL )
    {
        match = _eventd_config_get_best_match(list, event, current_flags);
        if ( match != NULL )
            return match;
    }

    list = g_hash_table_lookup(config->event_ids, category);
    if ( list != NULL )
    {
        match = _eventd_config_get_best_match(list, event, current_flags);
        if ( match != NULL )
            return match;
    }

    return NULL;
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
_eventd_config_match_data_match_free(gpointer data)
{
    EventdConfigDataMatch *data_match = data;

    g_regex_unref(data_match->regex);
    g_free(data_match->data);

    g_free(data_match);
}

static void
_eventd_config_match_free(gpointer data)
{
    EventdConfigMatch *match = data;

    g_list_free_full(match->if_data_matches, _eventd_config_match_data_match_free);
    g_strfreev(match->if_data);

    g_free(match->id);

    g_free(match);
}

static void
_eventd_config_matches_free(gpointer data)
{
    g_list_free_full(data, _eventd_config_match_free);
}

static gint
_eventd_config_compare_matches(gconstpointer a_, gconstpointer b_)
{
    const EventdConfigMatch *a = a_;
    const EventdConfigMatch *b = b_;
    if ( a->importance < b->importance )
        return -1;
    if ( b->importance < a->importance )
        return 1;
    return 0;
}

static GQuark *
_eventd_config_parse_event_flags(gchar **flags, gsize length)
{
    GQuark *quarks;

    quarks = g_new0(GQuark, length + 1);
    quarks[length] = 0;

    gsize i;
    for ( i = 0 ; i < length ; ++i )
    {
        gchar *flag = flags[i];
        quarks[i] = g_quark_from_string(flag);
        g_free(flag);
    }

    g_free(flags);

    return quarks;
}

static void
_eventd_config_parse_event_file(EventdConfig *config, const gchar *id, GKeyFile *config_file)
{
    gchar *category = NULL;
    gchar *name = NULL;

#ifdef DEBUG
    g_debug("Parsing event '%s'", id);
#endif /* DEBUG */

    if ( libeventd_config_key_file_get_string(config_file, "Event", "Category", &category) != 0 )
        return;

    if ( libeventd_config_key_file_get_string(config_file, "Event", "Name", &name) < 0 )
        goto fail;

    EventdConfigMatch *match;
    match = g_new0(EventdConfigMatch, 1);
    match->id = g_strdup(id);

    gchar **if_data_matches;
    gchar **flags;
    gsize length;

    libeventd_config_key_file_get_string_list(config_file, "Event", "IfData", &match->if_data, NULL);

    if ( libeventd_config_key_file_get_string_list(config_file, "Event", "IfDataMatches", &if_data_matches, NULL) == 0 )
    {
        EventdConfigDataMatch *data_match;
        gchar **if_data_match;
        GError *error = NULL;
        GRegex *regex;

        for ( if_data_match = if_data_matches ; *if_data_match != NULL ; ++if_data_match )
        {
            gchar **if_data_matchv;
            if_data_matchv = g_strsplit(*if_data_match, ",", 2);
            if ( ( regex = g_regex_new(if_data_matchv[1], G_REGEX_OPTIMIZE, 0, &error) ) != NULL )
            {
                data_match = g_new0(EventdConfigDataMatch, 1);
                data_match->data = g_strdup(if_data_matchv[0]);
                data_match->regex = regex;

                match->if_data_matches = g_list_prepend(match->if_data_matches, data_match);
            }
            g_strfreev(if_data_matchv);
        }
        g_strfreev(if_data_matches);
    }

    if ( libeventd_config_key_file_get_string_list(config_file, "Event", "OnlyIfFlags", &flags, &length) == 0 )
        match->flags_whitelist = _eventd_config_parse_event_flags(flags, length);

    if ( libeventd_config_key_file_get_string_list(config_file, "Event", "NotIfFlags", &flags, &length) == 0 )
        match->flags_blacklist = _eventd_config_parse_event_flags(flags, length);

    gint64 default_importance;

    if ( ( match->if_data != NULL ) || ( match->if_data_matches != NULL ) || ( match->flags_whitelist != NULL ) || ( match->flags_blacklist != NULL ) )
        default_importance = 0;
    else
        default_importance = G_MAXINT64;
    libeventd_config_key_file_get_int_with_default(config_file, "Event", "Importance", default_importance, &match->importance);

    gchar *internal_name;

    internal_name = _eventd_config_events_get_name(category, name);

    GList *list;

    list = g_hash_table_lookup(config->event_ids, internal_name);
    g_hash_table_steal(config->event_ids, internal_name);
    list = g_list_insert_sorted(list, match, _eventd_config_compare_matches);
    g_hash_table_insert(config->event_ids, internal_name, list);

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
            else if ( ! g_key_file_has_group(config_file, "Event") )
                g_key_file_free(config_file);
            else
                g_hash_table_insert(config_files, g_strdup(file), config_file);
        }
        else if ( g_file_test(config_file_name, G_FILE_TEST_IS_DIR) )
            _eventd_config_read_dir(config, config_files, config_file_name);

        g_free(config_file_name);
    }
    g_dir_close(config_dir);
}

static void
_eventd_config_load_dir(EventdConfig *config, GHashTable *config_files, const gchar *config_dir_name)
{
    GError *error = NULL;
    gchar *config_file_name = NULL;

    if ( ! g_file_test(config_dir_name, G_FILE_TEST_IS_DIR) )
        return;

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

    GKeyFile *new_config_file = NULL;

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

fail:
    g_free(parent_id);

    return new_config_file;
}

EventdConfig *
eventd_config_new(void)
{
    EventdConfig *config;

    config = g_new0(EventdConfig, 1);

    config->event_ids = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, _eventd_config_matches_free);
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
    eventd_plugins_config_init_all();

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
