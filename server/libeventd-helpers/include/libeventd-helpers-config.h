/*
 * libeventd - Internal helper
 *
 * Copyright © 2011-2017 Quentin "Sardem FF7" Glidic
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

#include "libeventd-event.h"

typedef struct _NkTokenList FormatString;
typedef const gchar *(*FormatStringReplaceCallback)(const gchar *name, const EventdEvent *event, gpointer user_data);

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

typedef struct _Filename Filename;
typedef enum {
    FILENAME_PROCESS_RESULT_NONE = 0,
    FILENAME_PROCESS_RESULT_URI,
    FILENAME_PROCESS_RESULT_DATA,
    FILENAME_PROCESS_RESULT_THEME,
} FilenameProcessResult;

/*
 * These functions will return:
 *     0 on success: the key is there and the value is valid
 *     1 on missing: the key is not there; for _with_default functions, the default value is returned
 *     -1 on error: the key is there and the value is invalid
 */
gint8 evhelpers_config_key_file_get_boolean(GKeyFile *config_file, const gchar *group, const gchar *key, gboolean *value);
gint8 evhelpers_config_key_file_get_string(GKeyFile *config_file, const gchar *group, const gchar *key, gchar **value);
gint8 evhelpers_config_key_file_get_string_with_default(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *default_value, gchar **value);
gint8 evhelpers_config_key_file_get_locale_string(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *locale, gchar **value);
gint8 evhelpers_config_key_file_get_locale_string_with_default(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *locale, const gchar *default_value, gchar **value);
gint8 evhelpers_config_key_file_get_int(GKeyFile *config_file, const gchar *group, const gchar *key, Int *value);
gint8 evhelpers_config_key_file_get_int_with_default(GKeyFile *config_file, const gchar *group, const gchar *key, gint64 default_value, gint64 *value);
gint8 evhelpers_config_key_file_get_int_list(GKeyFile *config_file, const gchar *group, const gchar *key, Int *value, gsize *length);
gint8 evhelpers_config_key_file_get_enum(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar * const *values, guint64 size, guint64 *value);
gint8 evhelpers_config_key_file_get_enum_with_default(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar * const *values, guint64 size, guint64 default_value, guint64 *value);
gint8 evhelpers_config_key_file_get_string_list(GKeyFile *config_file, const gchar *group, const gchar *key, gchar ***value, gsize *length);
gint8 evhelpers_config_key_file_get_format_string(GKeyFile *config_file, const gchar *group, const gchar *key, FormatString **value);
gint8 evhelpers_config_key_file_get_format_string_with_default(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *default_value, FormatString **value);
gint8 evhelpers_config_key_file_get_locale_format_string(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *locale, FormatString **value);
gint8 evhelpers_config_key_file_get_locale_format_string_with_default(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *locale, const gchar *default_value, FormatString **value);
gint8 evhelpers_config_key_file_get_filename(GKeyFile *config_file, const gchar *group, const gchar *key, Filename **value);
gint8 evhelpers_config_key_file_get_filename_with_default(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *default_value, Filename **value);
gint8 evhelpers_config_key_file_get_locale_filename(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *locale, Filename **value);
gint8 evhelpers_config_key_file_get_locale_filename_with_default(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *locale, const gchar *default_value, Filename **value);
gint8 evhelpers_config_key_file_get_colour(GKeyFile *config_file, const gchar *group, const gchar *key, Colour *value);

gchar *evhelpers_format_string_get_string(const FormatString *format_string, EventdEvent *event, FormatStringReplaceCallback callback, gpointer user_data);
FilenameProcessResult evhelpers_filename_process(const Filename *filename, EventdEvent *event, const gchar *subdir, gchar **uri, GVariant **data);

FormatString *evhelpers_format_string_new(gchar *string);
FormatString *evhelpers_format_string_ref(FormatString *format_string);
void evhelpers_format_string_unref(FormatString *format_string);

Filename *evhelpers_filename_new(gchar *string);
Filename *evhelpers_filename_ref(Filename *filename);
void evhelpers_filename_unref(Filename *filename);

#endif /* __LIBEVENTD_CONFIG_H__ */
