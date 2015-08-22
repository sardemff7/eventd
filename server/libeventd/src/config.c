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

#include <nkutils-enum.h>
#include <nkutils-token.h>
#include <nkutils-colour.h>

#include <libeventd-config.h>

struct _Filename {
    guint64 ref_count;
    gchar *data_name;
    FormatString *file_uri;
};

EVENTD_EXPORT
FormatString *
libeventd_format_string_new(gchar *string)
{
    return nk_token_list_parse(string);
}

EVENTD_EXPORT
FormatString *
libeventd_format_string_ref(FormatString *format_string)
{
    if ( format_string == NULL )
        return NULL;
    return nk_token_list_ref(format_string);
}

EVENTD_EXPORT
void
libeventd_format_string_unref(FormatString *format_string)
{
    if ( format_string == NULL )
        return;
    nk_token_list_unref(format_string);
}

EVENTD_EXPORT
Filename *
libeventd_filename_new(gchar *string)
{
    gchar *data_name = NULL;
    FormatString *file_uri = NULL;

    if ( g_str_has_prefix(string, "file://") )
        file_uri = libeventd_format_string_new(string);
    else if ( g_utf8_strchr(string, -1, ' ') == NULL )
        data_name = string;
    else
    {
        g_free(string);
        return NULL;
    }
    Filename *filename;

    filename = g_new0(Filename, 1);
    filename->ref_count = 1;

    filename->data_name = data_name;
    filename->file_uri = file_uri;

    return filename;
}

EVENTD_EXPORT
Filename *
libeventd_filename_ref(Filename *filename)
{
    if ( filename != NULL )
        ++filename->ref_count;
    return filename;
}

EVENTD_EXPORT
void
libeventd_filename_unref(Filename *filename)
{
    if ( filename == NULL )
        return;

    if ( --filename->ref_count > 0 )
        return;

    libeventd_format_string_unref(filename->file_uri);
    g_free(filename->data_name);

    g_free(filename);
}


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
libeventd_config_key_file_get_enum(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar * const *values, guint64 size, guint64 *value)
{
    gint8 r;
    gchar *string;

    r = libeventd_config_key_file_get_string(config_file, group, key, &string);
    if ( r != 0 )
        return r;

    if ( ! nk_enum_parse(string, values, size, TRUE, value) )
        r = -1;

    g_free(string);

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

static gint8
_libeventd_config_key_file_get_format_string(gchar *string, FormatString **format_string)
{
    libeventd_format_string_unref(*format_string);
    *format_string = libeventd_format_string_new(string);

    return 0;
}

EVENTD_EXPORT
gint8
libeventd_config_key_file_get_format_string(GKeyFile *config_file, const gchar *group, const gchar *key, FormatString **value)
{
    gchar *string;
    gint8 r;

    if ( ( r = libeventd_config_key_file_get_string(config_file, group, key, &string) ) != 0 )
        return r;

    return _libeventd_config_key_file_get_format_string(string, value);
}

EVENTD_EXPORT
gint8
libeventd_config_key_file_get_format_string_with_default(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *default_value, FormatString **value)
{
    gchar *string;
    gint8 r;

    if ( ( r = libeventd_config_key_file_get_string_with_default(config_file, group, key, default_value, &string) ) != 0 ) {
        g_free(string);
        return r;
    }

    return _libeventd_config_key_file_get_format_string(string, value);
}

EVENTD_EXPORT
gint8
libeventd_config_key_file_get_locale_format_string(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *locale, FormatString **value)
{
    gchar *string;
    gint8 r;

    if ( ( r = libeventd_config_key_file_get_locale_string(config_file, group, key, locale, &string) ) != 0 )
        return r;

    return _libeventd_config_key_file_get_format_string(string, value);
}

EVENTD_EXPORT
gint8
libeventd_config_key_file_get_locale_format_string_with_default(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *locale, const gchar *default_value, FormatString **value)
{
    gchar *string;
    gint8 r;

    if ( ( r = libeventd_config_key_file_get_locale_string_with_default(config_file, group, key, locale, default_value, &string) ) != 0 ) {
        g_free(string);
        return r;
    }

    return _libeventd_config_key_file_get_format_string(string, value);
}

static gint8
_libeventd_config_key_file_get_filename(gchar *string, Filename **value)
{
    Filename *filename;

    filename = libeventd_filename_new(string);
    if ( filename == NULL )
        return 1;

    libeventd_filename_unref(*value);
    *value = filename;

    return 0;
}

EVENTD_EXPORT
gint8
libeventd_config_key_file_get_filename(GKeyFile *config_file, const gchar *group, const gchar *key, Filename **value)
{
    gchar *string;
    gint8 r;

    if ( ( r = libeventd_config_key_file_get_string(config_file, group, key, &string) ) != 0 )
        return r;

    return _libeventd_config_key_file_get_filename(string, value);
}

EVENTD_EXPORT
gint8
libeventd_config_key_file_get_filename_with_default(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *default_value, Filename **value)
{
    gchar *string;
    gint8 r;

    if ( ( r = libeventd_config_key_file_get_string_with_default(config_file, group, key, default_value, &string) ) != 0 ) {
        g_free(string);
        return r;
    }

    return _libeventd_config_key_file_get_filename(string, value);
}

EVENTD_EXPORT
gint8
libeventd_config_key_file_get_locale_filename(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *locale, Filename **value)
{
    gchar *string;
    gint8 r;

    if ( ( r = libeventd_config_key_file_get_locale_string(config_file, group, key, locale, &string) ) != 0 )
        return r;

    return _libeventd_config_key_file_get_filename(string, value);
}

EVENTD_EXPORT
gint8
libeventd_config_key_file_get_locale_filename_with_default(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *locale, const gchar *default_value, Filename **value)
{
    gchar *string;
    gint8 r;

    if ( ( r = libeventd_config_key_file_get_locale_string_with_default(config_file, group, key, locale, default_value, &string) ) != 0 ) {
        g_free(string);
        return r;
    }

    return _libeventd_config_key_file_get_filename(string, value);
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


typedef struct {
    EventdEvent *event;
    FormatStringReplaceCallback callback;
    gconstpointer user_data;
} FormatStringReplaceData;

static const gchar *
_libeventd_token_list_callback(const gchar *token, guint64 value, gconstpointer user_data)
{
    const FormatStringReplaceData *data = user_data;

    if ( data->callback != NULL )
        return data->callback(token, data->event, data->user_data);

    return eventd_event_get_data(data->event, token);
}

EVENTD_EXPORT
gchar *
libeventd_format_string_get_string(const FormatString *format_string, EventdEvent *event, FormatStringReplaceCallback callback, gconstpointer user_data)
{
    if ( format_string == NULL )
        return NULL;

    FormatStringReplaceData data;

    data.event = event;
    data.callback = callback;
    data.user_data = user_data;

    return nk_token_list_replace(format_string, _libeventd_token_list_callback, &data);
}

EVENTD_EXPORT
gboolean
libeventd_filename_get_path(const Filename *filename, EventdEvent *event, const gchar *subdir, const gchar **ret_data, gchar **ret_path)
{
    g_return_val_if_fail(filename != NULL, FALSE);
    g_return_val_if_fail(event != NULL, FALSE);
    g_return_val_if_fail(subdir != NULL, FALSE);
    g_return_val_if_fail(ret_path != NULL, FALSE);

    const gchar *data = NULL;
    const gchar *path = NULL;
    gchar *path_ = NULL;

    if ( filename->data_name != NULL )
    {
        data = filename->data_name;
        path = eventd_event_get_data(event, data);
        if ( ( path == NULL ) || ( ! g_str_has_prefix(path, "file://") ) )
            return FALSE;
    }
    else if ( filename->file_uri != NULL )
        path = path_ = libeventd_format_string_get_string(filename->file_uri, event, NULL, NULL);
    else
    {
        g_assert_not_reached();
        return FALSE;
    }
    path += strlen("file://");

    if ( ! g_path_is_absolute(path) )
    {
        gchar *tmp = path_;
        path = path_ = g_build_filename(g_get_user_data_dir(), PACKAGE_NAME, subdir, path, NULL);
        g_free(tmp);
    }

    if ( g_file_test(path, G_FILE_TEST_IS_REGULAR) )
        *ret_path = g_strdup(path);
    else
        *ret_path = NULL;
    if ( ret_data != NULL )
        *ret_data = data;

    g_free(path_);

    return TRUE;
}
