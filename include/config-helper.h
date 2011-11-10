/*
 * libeventd-config-helper - Internal helper for config
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

#ifndef __EVENTD_CONFIG_HELPER_H__
#define __EVENTD_CONFIG_HELPER_H__

typedef struct {
    gint64 value;
    gboolean set;
} Int;

gint8 eventd_config_helper_key_file_get_boolean(GKeyFile *config_file, const gchar *group, const gchar *key, gboolean *value);
gint8 eventd_config_helper_key_file_get_string(GKeyFile *config_file, const gchar *group, const gchar *key, gchar **value);
gint8 eventd_config_helper_key_file_get_int(GKeyFile *config_file, const gchar *group, const gchar *key, Int *value);

#endif /* __EVENTD_CONFIG_HELPER_H__ */
