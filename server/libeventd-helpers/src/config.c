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

#include <config.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <libeventd-event.h>

#include <nkutils-enum.h>
#include <nkutils-token.h>
#include <nkutils-colour.h>

#include <libeventd-helpers-config.h>

struct _Filename {
    guint64 ref_count;
    gchar *data_name;
    FormatString *file_uri;
};

EVENTD_EXPORT
FormatString *
evhelpers_format_string_new(gchar *string)
{
    return nk_token_list_parse(string);
}

EVENTD_EXPORT
FormatString *
evhelpers_format_string_ref(FormatString *format_string)
{
    if ( format_string == NULL )
        return NULL;
    return nk_token_list_ref(format_string);
}

EVENTD_EXPORT
void
evhelpers_format_string_unref(FormatString *format_string)
{
    if ( format_string == NULL )
        return;
    nk_token_list_unref(format_string);
}

static gboolean
_evhelpers_filename_check_data_base64_prefix(const gchar *string)
{
    gchar *c;
    gsize s;

    c = g_utf8_strchr(string, -1, ',');
    s = c - string;
    if ( ( c == NULL ) || ( s < strlen(";base64") ) )
        return FALSE;

    c = g_utf8_strrchr(string, s, ';');
    if ( ( c == NULL ) || ( ! g_str_has_prefix(c, ";base64") ) )
        return FALSE;

    return TRUE;
}

EVENTD_EXPORT
Filename *
evhelpers_filename_new(gchar *string)
{
    gchar *data_name = NULL;
    FormatString *file_uri = NULL;

    if ( g_str_has_prefix(string, "file://") )
        file_uri = evhelpers_format_string_new(string);
    else if ( g_str_has_prefix(string, "data:") )
    {
        if ( ! _evhelpers_filename_check_data_base64_prefix(string + strlen("data:")) )
        {
            g_free(string);
            return NULL;
        }
        file_uri = evhelpers_format_string_new(string);
    }
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
evhelpers_filename_ref(Filename *filename)
{
    if ( filename != NULL )
        ++filename->ref_count;
    return filename;
}

EVENTD_EXPORT
void
evhelpers_filename_unref(Filename *filename)
{
    if ( filename == NULL )
        return;

    if ( --filename->ref_count > 0 )
        return;

    evhelpers_format_string_unref(filename->file_uri);
    g_free(filename->data_name);

    g_free(filename);
}


static gint8
_evhelpers_config_key_file_error(GError **error, const gchar *group, const gchar *key)
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
evhelpers_config_key_file_get_boolean(GKeyFile *config_file, const gchar *group, const gchar *key, gboolean *value)
{
    GError *error = NULL;

    *value = g_key_file_get_boolean(config_file, group, key, &error);

    return _evhelpers_config_key_file_error(&error, group, key);
}

EVENTD_EXPORT
gint8
evhelpers_config_key_file_get_int(GKeyFile *config_file, const gchar *group, const gchar *key, Int *value)
{
    GError *error = NULL;

    value->value = g_key_file_get_int64(config_file, group, key, &error);
    value->set = ( error == NULL );

    return _evhelpers_config_key_file_error(&error, group, key);
}

EVENTD_EXPORT
gint8
evhelpers_config_key_file_get_int_with_default(GKeyFile *config_file, const gchar *group, const gchar *key, gint64 default_value, gint64 *ret_value)
{
    gint8 r;
    Int value;

    r = evhelpers_config_key_file_get_int(config_file, group, key, &value);
    if ( r >= 0 )
        *ret_value = value.set ? value.value : default_value;

    return r;
}

EVENTD_EXPORT
gint8
evhelpers_config_key_file_get_string(GKeyFile *config_file, const gchar *group, const gchar *key, gchar **value)
{
    GError *error = NULL;

    *value = g_key_file_get_string(config_file, group, key, &error);

    return _evhelpers_config_key_file_error(&error, group, key);
}

EVENTD_EXPORT
gint8
evhelpers_config_key_file_get_string_with_default(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *default_value, gchar **ret_value)
{
    gint8 r;

    r = evhelpers_config_key_file_get_string(config_file, group, key, ret_value);
    if ( r > 0 )
        *ret_value = g_strdup(default_value);

    return r;
}

EVENTD_EXPORT
gint8
evhelpers_config_key_file_get_locale_string(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *locale, gchar **value)
{
    GError *error = NULL;

    *value = g_key_file_get_locale_string(config_file, group, key, locale, &error);

    return _evhelpers_config_key_file_error(&error, group, key);
}

EVENTD_EXPORT
gint8
evhelpers_config_key_file_get_locale_string_with_default(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *locale, const gchar *default_value, gchar **ret_value)
{
    gint8 r;

    r = evhelpers_config_key_file_get_locale_string(config_file, group, key, locale, ret_value);
    if ( r > 0 )
        *ret_value = g_strdup(default_value);

    return r;
}

EVENTD_EXPORT
gint8
evhelpers_config_key_file_get_enum(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar * const *values, guint64 size, guint64 *value)
{
    gint8 r;
    gchar *string;

    r = evhelpers_config_key_file_get_string(config_file, group, key, &string);
    if ( r != 0 )
        return r;

    if ( ! nk_enum_parse(string, values, size, TRUE, value) )
        r = -1;

    g_free(string);

    return r;
}

EVENTD_EXPORT
gint8
evhelpers_config_key_file_get_enum_with_default(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar * const *values, guint64 size, guint64 default_value, guint64 *value)
{
    gint8 r;

    r = evhelpers_config_key_file_get_enum(config_file, group, key, values, size, value);
    if ( r > 0 )
        *value = default_value;

    return r;
}

EVENTD_EXPORT
gint8
evhelpers_config_key_file_get_string_list(GKeyFile *config_file, const gchar *group, const gchar *key, gchar ***value, gsize *length)
{
    GError *error = NULL;

    *value = g_key_file_get_string_list(config_file, group, key, length, &error);

    return _evhelpers_config_key_file_error(&error, group, key);
}

static gint8
_evhelpers_config_key_file_get_format_string(gchar *string, FormatString **format_string, gint8 r)
{
    if ( r < 0 )
        return r;

    evhelpers_format_string_unref(*format_string);
    *format_string = evhelpers_format_string_new(string);

    return r;
}

EVENTD_EXPORT
gint8
evhelpers_config_key_file_get_format_string(GKeyFile *config_file, const gchar *group, const gchar *key, FormatString **value)
{
    gchar *string;
    gint8 r;

    if ( ( r = evhelpers_config_key_file_get_string(config_file, group, key, &string) ) != 0 )
        return r;

    return _evhelpers_config_key_file_get_format_string(string, value, 0);
}

EVENTD_EXPORT
gint8
evhelpers_config_key_file_get_format_string_with_default(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *default_value, FormatString **value)
{
    gchar *string;
    gint8 r;

    r = evhelpers_config_key_file_get_string_with_default(config_file, group, key, default_value, &string);

    return _evhelpers_config_key_file_get_format_string(string, value, r);
}

EVENTD_EXPORT
gint8
evhelpers_config_key_file_get_locale_format_string(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *locale, FormatString **value)
{
    gchar *string;
    gint8 r;

    if ( ( r = evhelpers_config_key_file_get_locale_string(config_file, group, key, locale, &string) ) != 0 )
        return r;

    return _evhelpers_config_key_file_get_format_string(string, value, 0);
}

EVENTD_EXPORT
gint8
evhelpers_config_key_file_get_locale_format_string_with_default(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *locale, const gchar *default_value, FormatString **value)
{
    gchar *string;
    gint8 r;

    r = evhelpers_config_key_file_get_locale_string_with_default(config_file, group, key, locale, default_value, &string);

    return _evhelpers_config_key_file_get_format_string(string, value, r);
}

static gint8
_evhelpers_config_key_file_get_filename(gchar *string, Filename **value, const gchar *group, const gchar *key, gint8 r)
{
    if ( r < 0 )
        return r;

    Filename *filename;

    filename = evhelpers_filename_new(string);
    if ( filename == NULL )
    {
        g_warning("Couldn't read the key [%s] '%s': Filename key must be either a data-name or a file:// URI format string", group, key);
        return -1;
    }

    evhelpers_filename_unref(*value);
    *value = filename;

    return r;
}

EVENTD_EXPORT
gint8
evhelpers_config_key_file_get_filename(GKeyFile *config_file, const gchar *group, const gchar *key, Filename **value)
{
    gchar *string;
    gint8 r;

    if ( ( r = evhelpers_config_key_file_get_string(config_file, group, key, &string) ) != 0 )
        return r;

    return _evhelpers_config_key_file_get_filename(string, value, group, key, 0);
}

EVENTD_EXPORT
gint8
evhelpers_config_key_file_get_filename_with_default(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *default_value, Filename **value)
{
    gchar *string;
    gint8 r;

    r = evhelpers_config_key_file_get_string_with_default(config_file, group, key, default_value, &string);

    return _evhelpers_config_key_file_get_filename(string, value, group, key, r);
}

EVENTD_EXPORT
gint8
evhelpers_config_key_file_get_locale_filename(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *locale, Filename **value)
{
    gchar *string;
    gint8 r;

    if ( ( r = evhelpers_config_key_file_get_locale_string(config_file, group, key, locale, &string) ) != 0 )
        return r;

    return _evhelpers_config_key_file_get_filename(string, value, group, key, 0);
}

EVENTD_EXPORT
gint8
evhelpers_config_key_file_get_locale_filename_with_default(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *locale, const gchar *default_value, Filename **value)
{
    gchar *string;
    gint8 r;

    r = evhelpers_config_key_file_get_locale_string_with_default(config_file, group, key, locale, default_value, &string);

    return _evhelpers_config_key_file_get_filename(string, value, group, key, r);
}

EVENTD_EXPORT
gint8
evhelpers_config_key_file_get_colour(GKeyFile *config_file, const gchar *section, const gchar *key, Colour *colour)
{
    gchar *string;
    gint8 r;

    if ( ( r = evhelpers_config_key_file_get_string(config_file, section, key, &string) ) != 0 )
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
_evhelpers_token_list_callback(const gchar *token, guint64 value, gconstpointer user_data)
{
    const FormatStringReplaceData *data = user_data;

    if ( data->callback != NULL )
        return data->callback(token, data->event, data->user_data);

    return eventd_event_get_data(data->event, token);
}

EVENTD_EXPORT
gchar *
evhelpers_format_string_get_string(const FormatString *format_string, EventdEvent *event, FormatStringReplaceCallback callback, gconstpointer user_data)
{
    if ( format_string == NULL )
        return NULL;

    FormatStringReplaceData data;

    data.event = event;
    data.callback = callback;
    data.user_data = user_data;

    return nk_token_list_replace(format_string, _evhelpers_token_list_callback, &data);
}

EVENTD_EXPORT
gchar *
evhelpers_filename_get_uri(const Filename *filename, EventdEvent *event, const gchar *subdir)
{
    g_return_val_if_fail(filename != NULL, NULL);
    g_return_val_if_fail(event != NULL, NULL);
    g_return_val_if_fail(subdir != NULL, NULL);

    gchar *uri = NULL;

    if ( filename->data_name != NULL )
        uri = g_strdup(eventd_event_get_data(event, filename->data_name));
    else if ( filename->file_uri != NULL )
        uri = evhelpers_format_string_get_string(filename->file_uri, event, NULL, NULL);
    else
    {
        g_assert_not_reached();
        return NULL;
    }
    if ( uri == NULL )
        return NULL;
    if ( *uri == '\0' )
    {
        g_free(uri);
        return NULL;
    }

    if ( g_str_has_prefix(uri, "data:") )
    {
        if ( _evhelpers_filename_check_data_base64_prefix(uri + strlen("data:")) )
            return uri;
    }
    else if ( g_str_has_prefix(uri, "file://") )
    {
        const gchar *p = uri + strlen("file://");
        if ( ! g_path_is_absolute(p) )
        {
            gchar *tmp = uri;
            uri = g_strconcat("file://", g_get_user_data_dir(), G_DIR_SEPARATOR_S PACKAGE_NAME G_DIR_SEPARATOR_S, subdir, G_DIR_SEPARATOR_S, p, NULL);
            g_free(tmp);
            p = uri + strlen("file://");
        }
        if ( g_file_test(p, G_FILE_TEST_IS_REGULAR) )
            return uri;
    }

    g_free(uri);

    return NULL;
}
