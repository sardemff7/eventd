/*
 * libeventd - Internal helper
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

#ifndef __EVENTD_LIBEVENTD_PLUGINS_H__
#define __EVENTD_LIBEVENTD_PLUGINS_H__

void libeventd_plugins_load(GList **plugins, const gchar *plugins_subdir, gpointer user_data);
void libeventd_plugins_unload(GList **plugins);

void libeventd_plugins_foreach(GList *plugins, GFunc func, gpointer user_data);

void libeventd_plugins_start_all(GList *plugins);
void libeventd_plugins_stop_all(GList *plugins);

void libeventd_plugins_control_command_all(GList *plugins, gchar *command);

void libeventd_plugins_config_init_all(GList *plugins);
void libeventd_plugins_config_clean_all(GList *plugins);

void libeventd_plugins_event_parse_all(GList *plugins, const gchar *type, const gchar *event, GKeyFile *config_file);
void libeventd_plugins_event_action_all(GList *plugins, EventdEvent *event);
void libeventd_plugins_event_pong_all(GList *plugins, EventdEvent *event);

#endif /* __EVENTD_LIBEVENTD_PLUGINS_H__ */
