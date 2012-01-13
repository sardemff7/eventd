/*
 * libeventd - Internal helper
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

#ifndef __LIBEVENTD_CONFIG_H__
#define __LIBEVENTD_CONFIG_H__

GHashTable *libeventd_config_events_new(GDestroyNotify value_destroy_func);
gchar *libeventd_config_events_get_name(const gchar *event_category, const gchar *event_type);
gpointer libeventd_config_events_get_event(GHashTable *events, const gchar *event_category, const gchar *event_type);

typedef struct {
    gint64 value;
    gboolean set;
} Int;

gint8 libeventd_config_key_file_get_boolean(GKeyFile *config_file, const gchar *group, const gchar *key, gboolean *value);
gint8 libeventd_config_key_file_get_string(GKeyFile *config_file, const gchar *group, const gchar *key, gchar **value);
gint8 libeventd_config_key_file_get_int(GKeyFile *config_file, const gchar *group, const gchar *key, Int *value);

gchar *libeventd_config_get_filename(const gchar *filename, GHashTable *data, const gchar *subdir);

#endif /* __LIBEVENTD_CONFIG_H__ */
