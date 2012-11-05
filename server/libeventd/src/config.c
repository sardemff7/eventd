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

#include <glib.h>
#include <glib-object.h>

#include <libeventd-event.h>
#include <libeventd-regex.h>

#include <libeventd-config.h>

static gint8
_libeventd_config_key_file_error(GError **error, const gchar *group, const gchar *key)
{
    gint8 ret = 1;

    if ( *error == NULL )
        return 0;

    if ( (*error)->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND )
    {
        ret = -1;
        g_warning("Couldn't read the key [%s] '%s': %s", group, key, (*error)->message);
    }
    g_clear_error(error);

    return ret;
}

EVENTD_EXPORT
gint8
libeventd_config_key_file_get_boolean(GKeyFile *config_file, const gchar *group, const gchar *key, gboolean *value)
{
    GError *error = NULL;

    *value = g_key_file_get_boolean(config_file, group, key, &error);

    return _libeventd_config_key_file_error(&error, group, key);
}

EVENTD_EXPORT
gint8
libeventd_config_key_file_get_int(GKeyFile *config_file, const gchar *group, const gchar *key, Int *value)
{
    GError *error = NULL;

    value->value = g_key_file_get_int64(config_file, group, key, &error);
    value->set = ( error == NULL );

    return _libeventd_config_key_file_error(&error, group, key);
}

EVENTD_EXPORT
gint8
libeventd_config_key_file_get_string(GKeyFile *config_file, const gchar *group, const gchar *key, gchar **value)
{
    GError *error = NULL;

    *value = g_key_file_get_string(config_file, group, key, &error);

    return _libeventd_config_key_file_error(&error, group, key);
}

EVENTD_EXPORT
gint8
libeventd_config_key_file_get_string_list(GKeyFile *config_file, const gchar *group, const gchar *key, gchar ***value, gsize *length)
{
    GError *error = NULL;

    *value = g_key_file_get_string_list(config_file, group, key, length, &error);

    return _libeventd_config_key_file_error(&error, group, key);
}

EVENTD_EXPORT
gint8
libeventd_config_key_file_get_colour(GKeyFile *config_file, const gchar *section, const gchar *key, Colour *colour)
{
    gchar *string;
    gint8 r;

    if ( ( r = libeventd_config_key_file_get_string(config_file, section, key, &string) ) != 0 )
        return r;

    r = 1;

    if ( string[0] == '#' )
    {
        gchar hex[3] = {0};

        hex[0] = string[1]; hex[1] = string[2];
        colour->r = g_ascii_strtoll(hex, NULL, 16) / 255.;

        hex[0] = string[3]; hex[1] = string[4];
        colour->g = g_ascii_strtoll(hex, NULL, 16) / 255.;

        hex[0] = string[5]; hex[1] = string[6];
        colour->b = g_ascii_strtoll(hex, NULL, 16) / 255.;

        if ( string[7] != 0 )
        {
            hex[0] = string[7]; hex[1] = string[8];
            colour->a = g_ascii_strtoll(hex, NULL, 16) / 255.;
        }
        else
            colour->a = 1.0;

        r = 0
    }
    else if ( g_str_has_prefix(string, "rgb(") && g_str_has_suffix(string, ")") )
        g_warning("rgb() format not yet supported");
    else if ( g_str_has_prefix(string, "rgba(") && g_str_has_suffix(string, ")") )
        g_warning("rgba() format not yet supported");

    g_free(string);

    return r;
}

EVENTD_EXPORT
gchar *
libeventd_config_get_filename(const gchar *filename, EventdEvent *event, const gchar *subdir)
{
    gchar *real_filename;

    if ( ! g_str_has_prefix(filename, "file://") )
    {
        filename = eventd_event_get_data(event, filename);
        if ( ( filename != NULL ) && g_str_has_prefix(filename, "file://") )
            real_filename = g_strdup(filename+7);
        else
            return NULL;
    }
    else
        real_filename = libeventd_regex_replace_event_data(filename+7, event, NULL, NULL);

    if ( ! g_path_is_absolute(real_filename) )
    {
        gchar *tmp;
        tmp = real_filename;
        real_filename = g_build_filename(g_get_user_data_dir(), PACKAGE_NAME, subdir, tmp, NULL);
        g_free(tmp);
    }

    if ( g_file_test(real_filename, G_FILE_TEST_IS_REGULAR) )
        return real_filename;

    g_free(real_filename);
    return g_strdup("");
}
