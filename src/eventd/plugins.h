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

#ifndef __EVENTD_PLUGINS_H__
#define __EVENTD_PLUGINS_H__

void eventd_plugins_load();
void eventd_plugins_unload();

void eventd_plugins_start_all();
void eventd_plugins_stop_all();

void eventd_plugins_control_command_all(gchar *command);

void eventd_plugins_config_reset_all();

void eventd_plugins_event_parse_all(const gchar *type, const gchar *event, GKeyFile *config_file);
void eventd_plugins_event_action_all(EventdEvent *event);
void eventd_plugins_event_pong_all(EventdEvent *event);

#endif /* __EVENTD_PLUGINS_H__ */
