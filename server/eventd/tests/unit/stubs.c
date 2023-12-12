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

#include <glib.h>

#include <libeventd-event.h>

#include "types.h"
#include "actions.h"
#include "plugins.h"

void eventd_plugins_config_reset_all(void) {}
void eventd_plugins_global_parse_all(GKeyFile *config_file) {}

void eventd_actions_reset(void) {}
void eventd_actions_parse(EventdActions *actions, GKeyFile *file, const gchar *default_id) {}
void eventd_actions_link_actions(EventdActions *actions) {}
void eventd_actions_replace_actions(EventdActions *actions, GList **list) {}
void eventd_actions_dump_actions(GString *dump, GList *actions) {}
gchar *eventd_actions_dump_action(EventdActions *self, const gchar *action_id) { return NULL; }
EventdActions *eventd_actions_new(void) { return NULL; }
void eventd_actions_free(EventdActions *action) {}
