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

#ifndef __EVENTD_ACTIONS_H__
#define __EVENTD_ACTIONS_H__


EventdActions *eventd_actions_new(void);
void eventd_actions_free(EventdActions *actions);

void eventd_actions_parse(EventdActions *actions, GKeyFile *file, const gchar *default_id);
void eventd_actions_link_actions(EventdActions *actions);
void eventd_actions_reset();

void eventd_actions_replace_actions(EventdActions *self, GList **list);

void eventd_actions_trigger(EventdCoreContext *core, const GList *actions, EventdEvent *event);

#endif /* __EVENTD_ACTIONS_H__ */
