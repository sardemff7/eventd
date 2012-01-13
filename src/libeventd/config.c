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

#include <glib.h>

#include <libeventd-regex.h>

#include <libeventd-config.h>

GHashTable *
libeventd_config_events_new(GDestroyNotify value_destroy_func)
{
    return g_hash_table_new_full(g_str_hash, g_str_equal, g_free, value_destroy_func);
}

gchar *
libeventd_config_events_get_name(const gchar *event_category, const gchar *event_name)
{
    gchar *name;
    if ( event_name != NULL )
        name = g_strconcat(event_category, "-", event_name, NULL);
    else
        name = g_strdup(event_category);
    return name;
}

gpointer
libeventd_config_events_get_event(GHashTable *events, const gchar *event_category, const gchar *event_name)
{
    gpointer ret = NULL;
    gchar *name;

    name = libeventd_config_events_get_name(event_category, event_name);
    ret = g_hash_table_lookup(events, name);
    g_free(name);
    if ( ret == NULL )
        ret = g_hash_table_lookup(events, event_category);

    return ret;
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
        g_warning("Can’t read the key [%s] '%s': %s", group, key, (*error)->message);
    }
    g_clear_error(error);

    return ret;
}

gint8
libeventd_config_key_file_get_boolean(GKeyFile *config_file, const gchar *group, const gchar *key, gboolean *value)
{
    GError *error = NULL;

    *value = g_key_file_get_boolean(config_file, group, key, &error);

    return _libeventd_config_key_file_error(&error, group, key);
}

gint8
libeventd_config_key_file_get_int(GKeyFile *config_file, const gchar *group, const gchar *key, Int *value)
{
    GError *error = NULL;

    value->value = g_key_file_get_int64(config_file, group, key, &error);
    value->set = ( error == NULL );

    return _libeventd_config_key_file_error(&error, group, key);
}

gint8
libeventd_config_key_file_get_string(GKeyFile *config_file, const gchar *group, const gchar *key, gchar **value)
{
    GError *error = NULL;

    *value = g_key_file_get_string(config_file, group, key, &error);

    return _libeventd_config_key_file_error(&error, group, key);
}

gchar *
libeventd_config_get_filename(const gchar *filename, GHashTable *event_data, const gchar *subdir)
{
    gchar *real_filename;

    if ( ! g_str_has_prefix(filename, "file://") )
    {
        filename = g_hash_table_lookup(event_data, filename);
        if ( ( filename != NULL ) && g_str_has_prefix(filename, "file://") )
            real_filename = g_strdup(filename+7);
        else
            return NULL;
    }
    else
        real_filename = libeventd_regex_replace_event_data(filename+7, event_data, NULL);

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
    return "";
}
