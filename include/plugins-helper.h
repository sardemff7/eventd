/*
 * libeventd-plugins-helper - Internal helper to deal with plugins
 *
 * Copyright © 2011 Quentin "Sardem FF7" Glidic
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

#ifndef __EVENTD_PLUGINS_HELPER_H__
#define __EVENTD_PLUGINS_HELPER_H__

void eventd_plugins_helper_load(GList **plugins, const gchar *plugins_subdir, gpointer user_data);
void eventd_plugins_helper_unload(GList **plugins);

void eventd_plugins_helper_foreach(GList *plugins, GFunc func, gpointer user_data);

void eventd_plugins_helper_config_init_all(GList *plugins);
void eventd_plugins_helper_config_clean_all(GList *plugins);

void eventd_plugins_helper_event_parse_all(GList *plugins, const gchar *type, const gchar *event, GKeyFile *config_file, GKeyFile *defaults_config_file);
void eventd_plugins_helper_event_action_all(GList *plugins, const gchar *client_type, const gchar *client_name, const gchar *event_type, const gchar *event_name, const GHashTable *event_data);

#endif /* __EVENTD_PLUGINS_HELPER_H__ */
