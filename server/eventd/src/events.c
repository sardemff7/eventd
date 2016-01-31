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

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <glib.h>
#include <glib-object.h>

#include <libeventd-event.h>
#include <libeventd-helpers-config.h>
#include <eventdctl.h>

#include "types.h"

#include "actions.h"

#include "events.h"

struct _EventdEvents {
    GHashTable *events;
};

typedef struct {
    gchar *data;
    GRegex *regex;
} EventdEventsEventDataMatch;

typedef struct {
    gint64 importance;
    GList *actions;

    /* Conditions */
    gchar **if_data;
    EventdEventsEventDataMatch *if_data_matches;
    GQuark *flags_whitelist;
    GQuark *flags_blacklist;
} EventdEventsEvent;

static void
_eventd_events_event_free(gpointer data)
{
    if ( data == NULL )
        return;

    EventdEventsEvent *self = data;

    if ( self->if_data_matches != NULL )
    {
        EventdEventsEventDataMatch *match;
        for ( match = self->if_data_matches ; match->data != NULL ; ++match )
        {
            g_regex_unref(match->regex);
            g_free(match->data);
        }
    }
    g_free(self->if_data_matches);

    g_strfreev(self->if_data);

    g_list_free(self->actions);

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
        EventdEventsEventDataMatch *match;
        const gchar *data;
        for ( match = self->if_data_matches ; match->data != NULL ; ++match )
        {
            if ( ( data = eventd_event_get_data(event, match->data) ) == NULL )
                continue;
            if ( ! g_regex_match(match->regex, data, 0, NULL) )
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
    const gchar *category, *name;
    gsize s;
    GList *list;
    EventdEventsEvent *match;

    category = eventd_event_get_category(event);
    name = eventd_event_get_name(event);
    s = strlen(category) + strlen(name) + 2;

    gchar full_name[s];

    g_snprintf(full_name, s, "%s %s", category, name);

    list = g_hash_table_lookup(self->events, full_name);
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
eventd_events_process_event(EventdEvents *self, EventdEvent *event, GQuark *flags, const GList **actions)
{
    EventdEventsEvent *config_event;
    config_event = _eventd_events_get_event(self, event, flags);

    if ( config_event == NULL )
        return FALSE;

    *actions = config_event->actions;

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

static void
_eventd_events_parse_group(EventdEvents *self, const gchar *group, GKeyFile *config_file)
{
    gchar *name = NULL;

    const gchar *s;
    gchar *c;
    s = group + strlen("Event ");
    if ( ( ( c = g_utf8_strchr(s, -1, ' ') ) != NULL ) && ( ( c = g_utf8_strchr(c+1, -1, ' ') ) != NULL ) )
        name = g_strndup(s, c - s);
    else
        name = g_strdup(s);
    g_strstrip(name);

    gboolean disable = FALSE;

    if ( ( evhelpers_config_key_file_get_boolean(config_file, group, "Disable", &disable) < 0 ) || disable )
        goto fail;

    gchar **actions = NULL;

    if ( evhelpers_config_key_file_get_string_list(config_file, group, "Actions", &actions, NULL) < 0 )
        goto fail;

#ifdef EVENTD_DEBUG
    g_debug("Parsing event: %s", s);
#endif /* EVENTD_DEBUG */

    EventdEventsEvent *event;
    event = g_new0(EventdEventsEvent, 1);

    if ( actions != NULL )
    {
        gchar **action;
        for ( action = actions ; *action != NULL ; ++action )
            event->actions = g_list_prepend(event->actions, *action);
        g_free(actions);
    }


    gchar **if_data_matches;
    gchar **flags;
    gsize length;

    evhelpers_config_key_file_get_string_list(config_file, group, "IfData", &event->if_data, NULL);

    if ( evhelpers_config_key_file_get_string_list(config_file, group, "IfDataMatches", &if_data_matches, &length) == 0 )
    {
        gchar **if_data_match;
        EventdEventsEventDataMatch *match;
        gchar *data, *regex_;
        GError *error = NULL;
        GRegex *regex;

        event->if_data_matches = g_new0(EventdEventsEventDataMatch, length + 1);
        match = event->if_data_matches;

        for ( if_data_match = if_data_matches ; *if_data_match != NULL ; ++if_data_match )
        {
            data = *if_data_match;
            regex_ = g_utf8_strchr(data, -1, ',');
            if ( regex_ == NULL )
            {
                g_warning("Data matches must be of the form 'data-name,regex'");
                continue;
            }
            *regex_ = '\0';
            ++regex_;

            regex = g_regex_new(regex_, G_REGEX_OPTIMIZE, 0, &error);
            if ( regex == NULL )
            {
                g_warning("Could not compile regex '%s': %s", regex_, error->message);
                g_clear_error(&error);
                continue;
            }

            match->data = g_strdup(data);
            match->regex = regex;
            ++match;
        }
        match->data = NULL;
        g_strfreev(if_data_matches);
    }

    if ( evhelpers_config_key_file_get_string_list(config_file, group, "OnlyIfFlags", &flags, &length) == 0 )
        event->flags_whitelist = _eventd_events_parse_event_flags(flags, length);

    if ( evhelpers_config_key_file_get_string_list(config_file, group, "NotIfFlags", &flags, &length) == 0 )
        event->flags_blacklist = _eventd_events_parse_event_flags(flags, length);

    gint64 default_importance;

    if ( ( event->if_data != NULL ) || ( event->if_data_matches != NULL ) || ( event->flags_whitelist != NULL ) || ( event->flags_blacklist != NULL ) )
        default_importance = 0;
    else
        default_importance = G_MAXINT64;
    evhelpers_config_key_file_get_int_with_default(config_file, group, "Importance", default_importance, &event->importance);

    GList *list = NULL;
    gchar *old_key = NULL;
    g_hash_table_lookup_extended(self->events, name, (gpointer *)&old_key, (gpointer *)&list);
    g_hash_table_steal(self->events, name);
    g_free(old_key);
    list = g_list_insert_sorted(list, event, _eventd_events_compare_event);
    g_hash_table_insert(self->events, name, list);
    name = NULL;

fail:
    g_free(name);
}

void
eventd_events_parse(EventdEvents *self, GKeyFile *config_file)
{
    gchar **groups, **group;

    groups = g_key_file_get_groups(config_file, NULL);
    if ( groups != NULL )
    {
        for ( group = groups ; *group != NULL ; ++group )
        {
            if ( g_str_has_prefix(*group, "Event ") )
                _eventd_events_parse_group(self, *group, config_file);
            g_free(*group);
        }
        g_free(groups);
    }
}

void
eventd_events_link_actions(EventdEvents *self, EventdActions *actions)
{
    GHashTableIter iter;
    gchar *id;
    GList *events;
    g_hash_table_iter_init(&iter, self->events);
    while ( g_hash_table_iter_next(&iter, (gpointer *)&id, (gpointer *)&events) )
    {
        GList *event_;
        for ( event_ = events ; event_ != NULL ; event_ = g_list_next(event_) )
        {
            EventdEventsEvent *event = event_->data;
            eventd_actions_replace_actions(actions, &event->actions);
        }
    }
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
