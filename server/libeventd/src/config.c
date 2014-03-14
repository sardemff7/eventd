/*
 * libeventd - Internal helper
 *
 * Copyright © 2011-2014 Quentin "Sardem FF7" Glidic
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <glib.h>
#include <glib-object.h>

#include <libeventd-event.h>
#include <libeventd-regex.h>

#include <nkutils-colour.h>

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
libeventd_config_key_file_get_int_with_default(GKeyFile *config_file, const gchar *group, const gchar *key, gint64 default_value, gint64 *ret_value)
{
    gint8 r;
    Int value;

    r = libeventd_config_key_file_get_int(config_file, group, key, &value);
    if ( r >= 0 )
        *ret_value = value.set ? value.value : default_value;

    return r;
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
libeventd_config_key_file_get_string_with_default(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *default_value, gchar **ret_value)
{
    gint8 r;

    r = libeventd_config_key_file_get_string(config_file, group, key, ret_value);
    if ( r > 0 )
        *ret_value = g_strdup(default_value);

    return r;
}

EVENTD_EXPORT
gint8
libeventd_config_key_file_get_locale_string(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *locale, gchar **value)
{
    GError *error = NULL;

    *value = g_key_file_get_locale_string(config_file, group, key, locale, &error);

    return _libeventd_config_key_file_error(&error, group, key);
}

EVENTD_EXPORT
gint8
libeventd_config_key_file_get_locale_string_with_default(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *locale, const gchar *default_value, gchar **ret_value)
{
    gint8 r;

    r = libeventd_config_key_file_get_locale_string(config_file, group, key, locale, ret_value);
    if ( r > 0 )
        *ret_value = g_strdup(default_value);

    return r;
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

    NkColourDouble colour_;
    if ( nk_colour_double_parse(string, &colour_) )
    {
        colour->r = colour_.red;
        colour->g = colour_.green;
        colour->b = colour_.blue;
        colour->a = colour_.alpha;
        r = 0;
    }

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
