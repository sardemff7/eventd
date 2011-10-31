/*
 * libeventd-plugin-helper - Internal helper for plugins
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

#include <plugin-helper.h>

static gint8
eventd_plugin_helper_config_key_file_error(GError **error, const gchar *group, const gchar *key)
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
eventd_plugin_helper_config_key_file_get_int(GKeyFile *config_file, const gchar *group, const gchar *key, Int *value)
{
    GError *error = NULL;

    value->value = g_key_file_get_int64(config_file, group, key, &error);
    value->set = ( error == NULL );

    return eventd_plugin_helper_config_key_file_error(&error, group, key);
}

gint8
eventd_plugin_helper_config_key_file_get_string(GKeyFile *config_file, const gchar *group, const gchar *key, gchar **value)
{
    GError *error = NULL;

    *value = g_key_file_get_string(config_file, group, key, &error);

    return eventd_plugin_helper_config_key_file_error(&error, group, key);
}


static guint64 regex_refcount = 0;

static GRegex *regex_client_name = NULL;
static GRegex *regex_event_data = NULL;

void
eventd_plugin_helper_regex_init()
{
    GError *error = NULL;
    if ( ++regex_refcount > 1 )
        return;

    regex_client_name = g_regex_new("\\$client-name", G_REGEX_OPTIMIZE, 0, &error);
    if ( ! regex_client_name )
        g_warning("Can’t create $client-name regex: %s", error->message);
    g_clear_error(&error);

    regex_event_data = g_regex_new("\\$event-data\\[([\\w-]+)\\]", G_REGEX_OPTIMIZE, 0, &error);
    if ( ! regex_event_data )
        g_warning("Can’t create $event-data regex: %s", error->message);
    g_clear_error(&error);
}

void
eventd_plugin_helper_regex_clean()
{
    if ( --regex_refcount > 0 )
        return;

    g_regex_unref(regex_event_data);
    g_regex_unref(regex_client_name);
}

gchar *
eventd_plugin_helper_regex_replace_client_name(const gchar *target, const gchar *client_name)
{
    GError *error = NULL;
    gchar *r;

    r = g_regex_replace_literal(regex_client_name, target, -1, 0, client_name ?: "" , 0, &error);
    if ( r == NULL )
    {
        r = g_strdup(target);
        g_warning("Can’t replace client name: %s", error->message);
    }
    g_clear_error(&error);


    return r;
}

static gboolean
eventd_plugin_helper_regex_event_data_cb(const GMatchInfo *info, GString *r, gpointer event_data)
{
    gchar *name;
    gchar *data = NULL;

    name = g_match_info_fetch(info, 1);
    if ( event_data != NULL )
        data = g_hash_table_lookup((GHashTable *)event_data, name);
    g_string_append(r, data ?: "");
    g_free(name);

    return FALSE;
}

gchar *
eventd_plugin_helper_regex_replace_event_data(const gchar *target, GHashTable *event_data, GRegexEvalCallback callback)
{
    GError *error = NULL;
    gchar *r;

    r = g_regex_replace_eval(regex_event_data, target, -1, 0, 0, callback ?: eventd_plugin_helper_regex_event_data_cb, (gpointer)event_data, &error);
    if ( r == NULL )
    {
        r = g_strdup(target);
        g_warning("Can’t replace event data: %s", error->message);
    }
    g_clear_error(&error);

    return r;
}
