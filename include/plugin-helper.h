/*
 * libeventd-plugin-helper - Internal helper for plugins
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

#ifndef __EVENTD_PLUGIN_HELPER_H__
#define __EVENTD_PLUGIN_HELPER_H__

gint8 eventd_plugin_helper_config_key_file_get_string(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *event, const gchar *type, gchar **value);

#endif /* __EVENTD_PLUGIN_HELPER_H__ */
