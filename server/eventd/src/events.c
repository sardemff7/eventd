/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2015 Quentin "Sardem FF7" Glidic
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

#include <libeventd-event.h>
#include <libeventd-helpers-config.h>
#include <eventdctl.h>

#include "types.h"

#include "plugins.h"

#include "events.h"

struct _EventdEvents {
    GHashTable *events;
};

typedef struct {
    gchar *data;
    GRegex *regex;
} EventdEventsEventDataMatch;

typedef struct {
    gint64 timeout;

    gint64 importance;
    GList *actions;

    /* Conditions */
    gchar **if_data;
    GList *if_data_matches;
    GQuark *flags_whitelist;
    GQuark *flags_blacklist;
} EventdEventsEvent;

static gchar *
_eventd_events_events_get_name(const gchar *category, const gchar *name)
{
    return ( name == NULL ) ? g_strdup(category) : g_strconcat(category, "-", name, NULL);
}

static void
_eventd_events_event_data_match_free(gpointer data)
{
    EventdEventsEventDataMatch *data_match = data;

    g_regex_unref(data_match->regex);
    g_free(data_match->data);

    g_free(data_match);
}

static void
_eventd_events_event_free(gpointer data)
{
    if ( data == NULL )
        return;

    EventdEventsEvent *self = data;

    g_list_free_full(self->if_data_matches, _eventd_events_event_data_match_free);
    g_strfreev(self->if_data);

    g_list_free_full(self->actions, eventd_plugins_action_free);

    g_free(self);
}

static void
_eventd_events_event_list_free(gpointer data)
{
    if ( data == NULL )
        return;

    g_list_free_full(data, _eventd_events_event_free);
}

static gboolean
_eventd_events_event_matches(EventdEventsEvent *self, EventdEvent *event, GQuark *current_flags)
{
    if ( self->if_data != NULL )
    {
        gchar **data;
        for ( data = self->if_data ; *data != NULL ; ++data )
        {
            if ( ! eventd_event_has_data(event, *data) )
                return FALSE;
        }
    }

    if ( self->if_data_matches != NULL )
    {
        GList *data_match_;
        const gchar *data;
        for ( data_match_ = self->if_data_matches ; data_match_ != NULL ; data_match_ = g_list_next(data_match_) )
        {
            EventdEventsEventDataMatch *data_match = data_match_->data;
            if ( ( data = eventd_event_get_data(event, data_match->data) ) == NULL )
                continue;
            if ( ! g_regex_match(data_match->regex, data, 0, NULL) )
                return FALSE;
        }
    }

    if ( current_flags != NULL )
    {
        GQuark *flag;
        if ( self->flags_whitelist != NULL )
        {
            GQuark *wflag;
            for ( wflag = self->flags_whitelist ; *wflag != 0 ; ++wflag )
            {
                for ( flag = current_flags ; *flag != 0 ; ++flag )
                {
                    if ( *flag != *wflag )
                        return FALSE;
                }
            }
        }

        if ( self->flags_blacklist != NULL )
        {
            GQuark *bflag;
            for ( bflag = self->flags_blacklist ; *bflag != 0 ; ++bflag )
            {
                for ( flag = current_flags ; *flag != 0 ; ++flag )
                {
                    if ( *flag == *bflag )
                        return FALSE;
                }
            }
        }
    }

    return TRUE;
}

static EventdEventsEvent *
_eventd_events_get_best_match(GList *list, EventdEvent *event, GQuark *current_flags)
{
    GList *self_;
    for ( self_ = list ; self_ != NULL ; self_ = g_list_next(self_) )
    {
        EventdEventsEvent *self = self_->data;
        if ( _eventd_events_event_matches(self, event, current_flags) )
            return self;
    }
    return NULL;
}

static EventdEventsEvent *
_eventd_events_get_event(EventdEvents *self, EventdEvent *event, GQuark *current_flags)
{
    const gchar *category;
    GList *list;
    EventdEventsEvent *match;

    category = eventd_event_get_category(event);

    gchar *name;
    name = _eventd_events_events_get_name(category, eventd_event_get_name(event));
    list = g_hash_table_lookup(self->events, name);
    g_free(name);


    if ( list != NULL )
    {
        match = _eventd_events_get_best_match(list, event, current_flags);
        if ( match != NULL )
            return match;
    }

    list = g_hash_table_lookup(self->events, category);
    if ( list != NULL )
    {
        match = _eventd_events_get_best_match(list, event, current_flags);
        if ( match != NULL )
            return match;
    }

    return NULL;
}

gboolean
eventd_events_process_event(EventdEvents *self, EventdEvent *event, GQuark *flags, gint64 config_timeout, const GList **actions)
{
    EventdEventsEvent *config_event;
    config_event = _eventd_events_get_event(self, event, flags);

    if ( config_event == NULL )
        return FALSE;

    *actions = config_event->actions;

    gint64 timeout;

    timeout = eventd_event_get_timeout(event);
    if ( timeout < 0 )
    {
        if ( ( config_event == NULL ) || ( config_event->timeout < 0 ) )
            timeout = config_timeout;
        else
            timeout = config_event->timeout;
    }
    eventd_event_set_timeout(event, timeout);

    return TRUE;
}

static gint
_eventd_events_compare_event(gconstpointer a_, gconstpointer b_)
{
    const EventdEventsEvent *a = a_;
    const EventdEventsEvent *b = b_;
    if ( a->importance < b->importance )
        return -1;
    if ( b->importance < a->importance )
        return 1;
    return 0;
}

static GQuark *
_eventd_events_parse_event_flags(gchar **flags, gsize length)
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

void
eventd_events_parse(EventdEvents *self, const gchar *id, GKeyFile *config_file)
{
    gchar *category = NULL;
    gchar *name = NULL;

    if ( evhelpers_config_key_file_get_string(config_file, "Event", "Category", &category) != 0 )
        return;

    if ( evhelpers_config_key_file_get_string(config_file, "Event", "Name", &name) < 0 )
        goto fail;

    gboolean disable = FALSE;

    if ( ( evhelpers_config_key_file_get_boolean(config_file, "Event", "Disable", &disable) < 0 ) || disable )
        goto fail;

    GList *actions;

    actions = eventd_plugins_event_parse_all(config_file);
    if ( actions == NULL )
        return;

#ifdef EVENTD_DEBUG
    g_debug("Parsing event '%s'", id);
#endif /* EVENTD_DEBUG */

    EventdEventsEvent *event;
    event = g_new0(EventdEventsEvent, 1);
    event->timeout = -1;
    event->actions = actions;

    Int timeout;

    if ( evhelpers_config_key_file_get_int(config_file, "Event", "Timeout", &timeout) == 0 )
        event->timeout = timeout.value;


    gchar **if_data_matches;
    gchar **flags;
    gsize length;

    evhelpers_config_key_file_get_string_list(config_file, "Event", "IfData", &event->if_data, NULL);

    if ( evhelpers_config_key_file_get_string_list(config_file, "Event", "IfDataMatches", &if_data_matches, NULL) == 0 )
    {
        EventdEventsEventDataMatch *data_match;
        gchar **if_data_match;
        GError *error = NULL;
        GRegex *regex;

        for ( if_data_match = if_data_matches ; *if_data_match != NULL ; ++if_data_match )
        {
            gchar **if_data_matchv;
            if_data_matchv = g_strsplit(*if_data_match, ",", 2);
            if ( ( regex = g_regex_new(if_data_matchv[1], G_REGEX_OPTIMIZE, 0, &error) ) != NULL )
            {
                data_match = g_new0(EventdEventsEventDataMatch, 1);
                data_match->data = g_strdup(if_data_matchv[0]);
                data_match->regex = regex;

                event->if_data_matches = g_list_prepend(event->if_data_matches, data_match);
            }
            g_strfreev(if_data_matchv);
        }
        g_strfreev(if_data_matches);
    }

    if ( evhelpers_config_key_file_get_string_list(config_file, "Event", "OnlyIfFlags", &flags, &length) == 0 )
        event->flags_whitelist = _eventd_events_parse_event_flags(flags, length);

    if ( evhelpers_config_key_file_get_string_list(config_file, "Event", "NotIfFlags", &flags, &length) == 0 )
        event->flags_blacklist = _eventd_events_parse_event_flags(flags, length);

    gint64 default_importance;

    if ( ( event->if_data != NULL ) || ( event->if_data_matches != NULL ) || ( event->flags_whitelist != NULL ) || ( event->flags_blacklist != NULL ) )
        default_importance = 0;
    else
        default_importance = G_MAXINT64;
    evhelpers_config_key_file_get_int_with_default(config_file, "Event", "Importance", default_importance, &event->importance);

    gchar *internal_name;

    internal_name = _eventd_events_events_get_name(category, name);

    GList *list = NULL;
    gchar *old_key = NULL;
    g_hash_table_lookup_extended(self->events, internal_name, (gpointer *)&old_key, (gpointer *)&list);
    g_hash_table_steal(self->events, internal_name);
    g_free(old_key);
    list = g_list_insert_sorted(list, event, _eventd_events_compare_event);
    g_hash_table_insert(self->events, internal_name, list);

fail:
    g_free(name);
    g_free(category);
}

void
eventd_events_reset(EventdEvents *self)
{
    g_hash_table_remove_all(self->events);
}

EventdEvents *
eventd_events_new(void)
{
    EventdEvents *self;

    self = g_new0(EventdEvents, 1);

    self->events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, _eventd_events_event_list_free);

    return self;
}

void
eventd_events_free(EventdEvents *self)
{
    g_hash_table_unref(self->events);

    g_free(self);
}
