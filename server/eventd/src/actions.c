/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2021 Quentin "Sardem FF7" Glidic
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

#include <glib.h>
#include <glib-object.h>

#include "libeventd-event.h"
#include "libeventd-helpers-config.h"

#include "types.h"

#include "eventd.h"
#include "config_.h"
#include "plugins.h"

#include "actions.h"

struct _EventdActions {
    GHashTable *actions; /* id => EventdActionsAction */
};

typedef struct {
    GQuark *add;
    GQuark *remove;
} EventdActionsFlagsAction;

typedef struct {
    gchar *id;

    EventdActionsFlagsAction flags;
    GList *actions; /* EventdPluginsActions */
    GList *subactions; /* EventdActionsAction */
} EventdActionsAction;

static void
_eventd_actions_action_free(gpointer data)
{
    EventdActionsAction *self = data;

    g_list_free(self->subactions);
    g_list_free_full(self->actions, eventd_plugins_action_free);

    g_free(self->id);

    g_slice_free(EventdActionsAction, self);
}


void
eventd_actions_parse(EventdActions *self, GKeyFile *file, const gchar *default_id)
{
    gchar *id = NULL;

    EventdActionsAction *action;
    action = g_slice_new0(EventdActionsAction);

    if ( g_key_file_has_group(file, "Action") )
    {
        evhelpers_config_key_file_get_string_with_default(file, "Action", "Name", default_id, &id);

        gchar **subactions = NULL;
        if ( evhelpers_config_key_file_get_string_list(file, "Action", "Subactions", &subactions, NULL) == 0 )
        {
            gchar **subaction;
            for ( subaction = subactions ; *subaction != NULL ; ++subaction )
                action->subactions = g_list_prepend(action->subactions, *subaction);
            g_free(subactions);
        }
    }
    else
        id = g_strdup(default_id);

    eventd_debug("Parsing action: %s", id);
    action->id = id;

    if ( g_key_file_has_group(file, "Flags") )
    {
        gchar **list;
        gsize length;
        if ( evhelpers_config_key_file_get_string_list(file, "Flags", "Add", &list, &length) == 0 )
            action->flags.add = eventd_config_parse_flags_list(list, length);
        if ( evhelpers_config_key_file_get_string_list(file, "Flags", "Remove", &list, &length) == 0 )
            action->flags.remove = eventd_config_parse_flags_list(list, length);
    }

    action->actions = eventd_plugins_event_parse_all(file);

    g_hash_table_replace(self->actions, action->id, action);
}

void
eventd_actions_link_actions(EventdActions *self)
{
    GHashTableIter iter;
    gchar *id;
    EventdActionsAction *action;
    g_hash_table_iter_init(&iter, self->actions);
    while ( g_hash_table_iter_next(&iter, (gpointer *)&id, (gpointer *)&action) )
    {
        if ( action->subactions != NULL )
            eventd_actions_replace_actions(self, &action->subactions);
    }
}

void
eventd_actions_reset(EventdActions *self)
{
    g_hash_table_remove_all(self->actions);
}

void
eventd_actions_replace_actions(EventdActions *self, GList **list)
{
    GList *ret = *list;

    GList *action = ret;
    while ( action != NULL )
    {
        GList *next = g_list_next(action);

        gchar *id = action->data;
        action->data = g_hash_table_lookup(self->actions, id);
        g_free(id);

        if ( action->data == NULL )
            ret = g_list_delete_link(ret, action);

        action = next;
    }

    *list = ret;
}

static void
_eventd_actions_dump_action(GString *dump, EventdActionsAction *action)
{
    g_string_append_printf(dump, "Name: %s", action->id);

    GQuark *flag;
    if ( action->flags.add != NULL )
    {
        g_string_append(dump, "\n    Adding flags:");
        for ( flag = action->flags.add ; *flag != 0 ; ++flag )
            g_string_append_printf(dump, " %s,", g_quark_to_string(*flag));
        g_string_truncate(dump, dump->len - 1);
    }
    if ( action->flags.remove != NULL )
    {
        g_string_append(dump, "\n    Removing flags:");
        for ( flag = action->flags.remove ; *flag != 0 ; ++flag )
            g_string_append_printf(dump, " %s,", g_quark_to_string(*flag));
        g_string_truncate(dump, dump->len - 1);
    }

    if ( action->actions != NULL )
        g_string_append(dump, "\n    Has plugin actions");

    if ( action->subactions != NULL )
    {
        GList *subaction_;
        g_string_append(dump, "\n    Subactions:");
        for ( subaction_ = action->subactions ; subaction_ != NULL ; subaction_ = g_list_next(subaction_) )
        {
            EventdActionsAction *subaction = subaction_->data;
            g_string_append_printf(dump, " %s,", subaction->id);
        }
        g_string_truncate(dump, dump->len - 1);
    }

    g_string_append_c(dump, '\n');
}

void
eventd_actions_dump_actions(GString *dump, GList *actions)
{
    GList *action_;
    for ( action_ = actions ; action_ != NULL ; action_ = g_list_next(action_) )
        _eventd_actions_dump_action(dump, action_->data);
}

gchar *
eventd_actions_dump_action(EventdActions *self, const gchar *action_id)
{
    EventdActionsAction *action = g_hash_table_lookup(self->actions, action_id);
    if ( action == NULL )
        return NULL;

    GString *dump = g_string_sized_new(1000);
    GList action_ = { .data = action };
    eventd_actions_dump_actions(dump, &action_);
    return g_string_free(dump, FALSE);
}

static void
_eventd_actions_trigger_flags(EventdCoreContext *core, EventdActionsFlagsAction *action)
{
    GQuark *flag;
    if ( action->add != NULL )
    {
        for ( flag = action->add ; *flag != 0 ; ++flag )
            eventd_core_flags_add(core, *flag);
    }
    if ( action->remove != NULL )
    {
        for ( flag = action->remove ; *flag != 0 ; ++flag )
            eventd_core_flags_remove(core, *flag);
    }
}

void
eventd_actions_trigger(EventdCoreContext *core, const GList *action_, EventdEvent *event)
{
    for ( ; action_ != NULL ; action_ = g_list_next(action_) )
    {
        EventdActionsAction *action = action_->data;
        g_debug("Triggering action %s", action->id);
        _eventd_actions_trigger_flags(core, &action->flags);
        eventd_plugins_event_action_all(action->actions, event);
        eventd_actions_trigger(core, action->subactions, event);
    }
}

EventdActions *
eventd_actions_new(void)
{
    EventdActions *self;

    self = g_new0(EventdActions, 1);

    self->actions = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, _eventd_actions_action_free);

    return self;
}

void
eventd_actions_free(EventdActions *self)
{
    g_hash_table_unref(self->actions);

    g_free(self);
}
