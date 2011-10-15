/*
 * eventd - Small daemon to act on remote or local events
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

#ifndef __EVENTD_DIALOGS_H__
#define __EVENTD_DIALOGS_H__

void eventd_dialogs_config_init();
void eventd_dialogs_config_clean();

void eventd_dialogs_event_parse(const gchar *type, const gchar *event, GKeyFile *config_file, GKeyFile *defaults_config_file);
void eventd_dialogs_event_action(const gchar *client_type, const gchar *client_name, const gchar *event_type, const gchar *event_name, const gchar *event_data);

#endif /* __EVENTD_DIALOGS_H__ */
