/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2018 Quentin "Sardem FF7" Glidic
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

#include "plugins.h"

#include "actions.h"

struct _EventdActions {
    GHashTable *actions; /* id => EventdActionsAction */
};

typedef struct {
    GList *actions; /* EventdPluginsActions */
    GList *subactions; /* EventdActionsAction */
} EventdActionsAction;

static void
_eventd_actions_action_free(gpointer data)
{
    EventdActionsAction *self = data;

    g_list_free(self->subactions);
    g_list_free_full(self->actions, eventd_plugins_action_free);

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

#ifdef EVENTD_DEBUG
    g_debug("Parsing action: %s", id);
#endif /* EVENTD_DEBUG */

    action->actions = eventd_plugins_event_parse_all(file);

    g_hash_table_replace(self->actions, id, action);
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

void
eventd_actions_trigger(const GList *action_, EventdEvent *event)
{
    for ( ; action_ != NULL ; action_ = g_list_next(action_) )
    {
        EventdActionsAction *action = action_->data;
        eventd_plugins_event_action_all(action->actions, event);
        eventd_actions_trigger(action->subactions, event);
    }
}

EventdActions *
eventd_actions_new(void)
{
    EventdActions *self;

    self = g_new0(EventdActions, 1);

    self->actions = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, _eventd_actions_action_free);

    return self;
}

void
eventd_actions_free(EventdActions *self)
{
    g_hash_table_unref(self->actions);

    g_free(self);
}
