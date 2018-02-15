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

#ifndef __EVENTD_CONFIG_H__
#define __EVENTD_CONFIG_H__

EventdConfig *eventd_config_new(const gchar *arg_dir, gboolean system_mode);
void eventd_config_parse(EventdConfig *config, gboolean system_mode);
void eventd_config_free(EventdConfig *config);

gboolean eventd_config_process_event(EventdConfig *self, EventdEvent *event, GQuark *flags, const GList **actions);

#endif /* __EVENTD_CONFIG_H__ */
