/*
 * libeventd - Internal helper
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

#include "config.h"

#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "libeventd-event.h"

#include <nkutils-enum.h>
#include <nkutils-token.h>
#include <nkutils-colour.h>

#include "libeventd-helpers-config.h"

struct _Filename {
    guint64 ref_count;
    gchar *data_name;
    FormatString *file_uri;
};

EVENTD_EXPORT
FormatString *
evhelpers_format_string_new(gchar *string)
{
    GError *error = NULL;
    FormatString *format_string;

    format_string = nk_token_list_parse(string, '$', &error);
    if ( format_string != NULL )
        return format_string;

    g_warning("Malformed format string: %s", error->message);
    g_clear_error(&error);
    return NULL;
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
    {
        file_uri = evhelpers_format_string_new(string);
        if ( file_uri == NULL )
            return NULL;
    }
    else if ( g_str_has_prefix(string, "data:") )
    {
        if ( ! _evhelpers_filename_check_data_base64_prefix(string + strlen("data:")) )
        {
            g_free(string);
            return NULL;
        }
        file_uri = evhelpers_format_string_new(string);
        if ( file_uri == NULL )
            return NULL;
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
evhelpers_config_key_file_get_boolean(GKeyFile *config_file, const gchar *group, const gchar *key, gboolean *ret_value)
{
    GError *error = NULL;
    gboolean value;

    value = g_key_file_get_boolean(config_file, group, key, &error);
    if ( error == NULL )
        *ret_value = value;

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
evhelpers_config_key_file_get_int_list(GKeyFile *config_file, const gchar *group, const gchar *key, Int *values, gsize *length)
{
    gint *value;
    gsize value_length;
    GError *error = NULL;
    gint8 ret;

    value = g_key_file_get_integer_list(config_file, group, key, &value_length, &error);
    ret = _evhelpers_config_key_file_error(&error, group, key);

    if ( ret != 0 )
        return ret;

    if ( value_length > *length )
    {
        g_warning("Couldn't read the key [%s] '%s': Too many values", group, key);
        g_free(value);
        return -1;
    }

    gsize i;
    for ( i = 0 ; i < value_length ; ++i )
    {
        values[i].set = TRUE;
        values[i].value = value[i];
    }
    for ( ; i < *length ; ++i )
        values[i].set = FALSE;

    *length = value_length;

    g_free(value);
    return 0;
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

    if ( ! nk_enum_parse(string, values, size, TRUE, FALSE, value) )
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
    {
        g_free(string);
        return r;
    }

    FormatString *new_format_string;
    new_format_string = evhelpers_format_string_new(string);
    if ( new_format_string == NULL )
        return -1;

    evhelpers_format_string_unref(*format_string);
    *format_string = new_format_string;

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

EVENTD_EXPORT
gint8
evhelpers_config_key_file_get_template(GKeyFile *config_file, const gchar *group, const gchar *key, FormatString **value)
{
    GError *error = NULL;
    gchar *filename;
    gint8 r;

    if ( ( r = evhelpers_config_key_file_get_string(config_file, group, key, &filename) ) != 0 )
        return r;
    r = -1;

    GFile *file;
    gsize size;
    gchar *string = NULL;

    file = g_file_new_for_commandline_arg(filename);

    GFileInfo *info;
    info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, NULL, &error);
    if ( info == NULL )
    {
        g_warning("Could not open file %s for reading: %s", filename, error->message);
        goto fail;

    }
    size = g_file_info_get_size(info);
    g_object_unref(info);

    GFileInputStream *in;
    in = g_file_read(file, NULL, &error);
    if ( in == NULL )
    {
        g_warning("Could not open file %s for reading: %s", filename, error->message);
        goto fail;
    }


    string = g_new(gchar, size + 1);
    string[size] = '\0';
    gsize read_size;
    if ( ! g_input_stream_read_all(G_INPUT_STREAM(in), string, size, &read_size, NULL, &error) )
    {
        g_warning("Could not read file %s: %s", filename, error->message);
        g_object_unref(in);
        goto fail;
    }
    g_object_unref(in);

    g_return_val_if_fail(size >= read_size, -1);
    if ( size > read_size )
        g_warning("File %s was supposed to be %zu bytes, read only %zu", filename, size, read_size);

    r = 0;
fail:
    g_object_unref(file);
    g_free(filename);
    g_clear_error(&error);
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

    NkColour colour_;
    if ( nk_colour_parse(string, &colour_) )
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
    gchar number[255];
    gchar *to_free;
    FormatStringReplaceCallback callback;
    gpointer user_data;
} FormatStringReplaceData;

static const gchar *
_evhelpers_token_list_string_from_gvariant(FormatStringReplaceData *data, GVariant *content, const gchar *key, gint64 index)
{
    if ( content == NULL )
        return NULL;

    while ( g_variant_is_of_type(content, G_VARIANT_TYPE_VARIANT) )
        content = g_variant_get_variant(content);

    if ( g_variant_is_of_type(content, G_VARIANT_TYPE_DICTIONARY) )
    {
        if ( key != NULL )
        {
            if ( ( key[0] != '\0' ) && ( g_utf8_get_char(key) != '@' ) )
                return _evhelpers_token_list_string_from_gvariant(data, g_variant_lookup_value(content, key, NULL), NULL, 0);
            return NULL;
        }
    }
    else if ( g_variant_is_of_type(content, G_VARIANT_TYPE_ARRAY) )
    {
        const gchar *joiner = ", ";
        gsize length;
        length = g_variant_n_children(content);

        if ( length == 0 )
            return NULL;

        if ( key != NULL )
        {
            if ( key[0] == '\0' )
            {
                GVariant *child = NULL;
                gsize i = ABS(index);
                if ( ( index < 0 ) && ( i <= length ) )
                    child = g_variant_get_child_value(content, length - i);
                else if ( ( index >= 0 ) && ( i < length ) )
                    child = g_variant_get_child_value(content, i);

                return _evhelpers_token_list_string_from_gvariant(data, child, NULL, 0);
            }

            if ( g_utf8_get_char(key) != '@' )
                return NULL;

            joiner = g_utf8_next_char(key);
        }

        gsize jl = strlen(joiner);

        gsize i;
        GString *ret;
        ret = g_string_sized_new(length * (10 + jl));
        for ( i = 0 ; i < length ; ++i )
        {
            g_string_append(g_string_append(ret, _evhelpers_token_list_string_from_gvariant(data, g_variant_get_child_value(content, i), NULL, 0)), joiner);

            g_free(data->to_free);
            data->to_free = NULL;
        }
        g_string_truncate(ret, ret->len - jl);
        data->to_free = g_string_free(ret, FALSE);
        return data->to_free;
    }

    if ( g_variant_is_of_type(content, G_VARIANT_TYPE_STRING) )
        return g_variant_get_string(content, NULL);

    if ( g_variant_is_of_type(content, G_VARIANT_TYPE_BOOLEAN) )
        return g_variant_get_boolean(content) ? "true" : NULL;

#define _evhelpers_check_type_with_format(l, U, GFormat) G_STMT_START { \
        if ( g_variant_is_of_type(content, G_VARIANT_TYPE_##U) ) \
        { \
            g_snprintf(data->number, sizeof(data->number), "%" GFormat, g_variant_get_##l(content)); \
            return data->number; \
        } \
    } G_STMT_END
#define _evhelpers_check_type(l, U) _evhelpers_check_type_with_format(l, U, G_G##U##_FORMAT)

    _evhelpers_check_type(int16, INT16);
    _evhelpers_check_type(int32, INT32);
    _evhelpers_check_type(int64, INT64);
    _evhelpers_check_type_with_format(byte, BYTE, "hhu");
    _evhelpers_check_type(uint16, UINT16);
    _evhelpers_check_type(uint32, UINT32);
    _evhelpers_check_type(uint64, UINT64);
    _evhelpers_check_type_with_format(double, DOUBLE, "lf");

#undef _evhelpers_check_type
#undef _evhelpers_check_type_with_format

    data->to_free = g_variant_print(content, FALSE);

    return data->to_free;
}

static const gchar *
_evhelpers_token_list_callback(const gchar *token, guint64 value, const gchar *key, gint64 index, gpointer user_data)
{
    FormatStringReplaceData *data = user_data;

    if ( data->callback != NULL )
        return data->callback(token, key, index, data->event, data->user_data);

    g_free(data->to_free);
    data->to_free = NULL;

    GVariant *content;
    content = eventd_event_get_data(data->event, token);

    return _evhelpers_token_list_string_from_gvariant(data, content, key, index);
}

EVENTD_EXPORT
gchar *
evhelpers_format_string_get_string(const FormatString *format_string, EventdEvent *event, FormatStringReplaceCallback callback, gpointer user_data)
{
    if ( format_string == NULL )
        return NULL;

    FormatStringReplaceData data = {
        .event = event,
        .callback = callback,
        .user_data = user_data,
    };

    gchar *ret;
    ret = nk_token_list_replace(format_string, _evhelpers_token_list_callback, &data);
    g_free(data.to_free);

    return ret;
}

EVENTD_EXPORT
FilenameProcessResult
evhelpers_filename_process(const Filename *filename, EventdEvent *event, const gchar *subdir, gchar **ret_uri, GVariant **ret_data)
{
    g_return_val_if_fail(filename != NULL, FILENAME_PROCESS_RESULT_NONE);
    g_return_val_if_fail(event != NULL, FILENAME_PROCESS_RESULT_NONE);
    g_return_val_if_fail(subdir != NULL, FILENAME_PROCESS_RESULT_NONE);
    g_return_val_if_fail(ret_uri != NULL, FILENAME_PROCESS_RESULT_NONE);
    g_return_val_if_fail(ret_data != NULL, FILENAME_PROCESS_RESULT_NONE);

    gchar *uri = NULL;

    if ( filename->data_name != NULL )
    {
        GVariant *data;
        data = eventd_event_get_data(event, filename->data_name);
        if ( data == NULL )
            return FILENAME_PROCESS_RESULT_NONE;
        if ( g_variant_is_of_type(data, G_VARIANT_TYPE_STRING) )
            uri = g_variant_dup_string(data, NULL);
        else if ( g_variant_is_of_type(data, G_VARIANT_TYPE("(msmsv)")) )
        {
            *ret_data = g_variant_ref(data);
            return FILENAME_PROCESS_RESULT_DATA;
        }
        else
            return FILENAME_PROCESS_RESULT_NONE;
    }
    else if ( filename->file_uri != NULL )
        uri = evhelpers_format_string_get_string(filename->file_uri, event, NULL, NULL);
    else
        g_return_val_if_reached(FILENAME_PROCESS_RESULT_NONE);

    if ( uri == NULL )
        return FILENAME_PROCESS_RESULT_NONE;
    if ( *uri == '\0' )
    {
        g_free(uri);
        return FILENAME_PROCESS_RESULT_NONE;
    }

    if ( g_str_has_prefix(uri, "data:") )
    {
        gchar *mime_type = uri + strlen("data:");
        if ( _evhelpers_filename_check_data_base64_prefix(mime_type) )
        {
            gchar *c;
            guchar *data;
            gsize length;

            /* We checked for ";base64," already */
            c = g_utf8_strchr(mime_type, -1, ',');
            *c = '\0';

            data = g_base64_decode(c + 1, &length);

            c = g_utf8_strchr(mime_type, c - mime_type, ';');
            *c++ = '\0';

            if ( *mime_type == '\0' )
                mime_type = NULL;

            if ( *c == '\0' )
                c = NULL;

            *ret_data = g_variant_new("(msmsv)", mime_type, c, g_variant_new_from_data(G_VARIANT_TYPE_BYTESTRING, data, length, FALSE, g_free, data));
            g_free(uri);
            return FILENAME_PROCESS_RESULT_DATA;
        }
    }
    else if ( g_str_has_prefix(uri, "file://") )
    {
        const gchar *p = uri + strlen("file://");
        if ( ! g_path_is_absolute(p) )
        {
            gchar *tmp = uri;
            if ( g_path_is_absolute(subdir) )
                uri = g_strconcat("file://", subdir, G_DIR_SEPARATOR_S, p, NULL);
            else
                uri = g_strconcat("file://", g_get_user_data_dir(), G_DIR_SEPARATOR_S PACKAGE_NAME G_DIR_SEPARATOR_S, subdir, G_DIR_SEPARATOR_S, p, NULL);
            g_free(tmp);
            p = uri + strlen("file://");
        }
        if ( g_file_test(p, G_FILE_TEST_IS_REGULAR) )
        {
            *ret_uri = uri;
            return FILENAME_PROCESS_RESULT_URI;
        }
    }
    else if ( g_str_has_prefix(uri, "theme:") )
    {
        *ret_uri = uri;
        return FILENAME_PROCESS_RESULT_THEME;
    }

    g_free(uri);

    return FILENAME_PROCESS_RESULT_NONE;
}
