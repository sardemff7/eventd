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

#ifndef __LIBEVENTD_CONFIG_H__
#define __LIBEVENTD_CONFIG_H__

#include <libeventd-event-types.h>

typedef struct {
    gint64 value;
    gboolean set;
} Int;

typedef struct {
    gdouble r;
    gdouble g;
    gdouble b;
    gdouble a;
} Colour;

gint8 libeventd_config_key_file_get_boolean(GKeyFile *config_file, const gchar *group, const gchar *key, gboolean *value);
gint8 libeventd_config_key_file_get_string(GKeyFile *config_file, const gchar *group, const gchar *key, gchar **value);
gint8 libeventd_config_key_file_get_string_with_default(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *default_value, gchar **value);
gint8 libeventd_config_key_file_get_locale_string(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *locale, gchar **value);
gint8 libeventd_config_key_file_get_locale_string_with_default(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *locale, const gchar *default_value, gchar **value);
gint8 libeventd_config_key_file_get_int(GKeyFile *config_file, const gchar *group, const gchar *key, Int *value);
gint8 libeventd_config_key_file_get_int_with_default(GKeyFile *config_file, const gchar *group, const gchar *key, gint64 default_value, gint64 *value);
gint8 libeventd_config_key_file_get_string_list(GKeyFile *config_file, const gchar *group, const gchar *key, gchar ***value, gsize *length);
gint8 libeventd_config_key_file_get_colour(GKeyFile *config_file, const gchar *group, const gchar *key, Colour *value);

gchar *libeventd_config_get_filename(const gchar *filename, EventdEvent *event, const gchar *subdir);

#endif /* __LIBEVENTD_CONFIG_H__ */
