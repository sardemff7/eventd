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

#include <libeventd-config.h>

static guint64 regex_refcount = 0;

static GRegex *regex_event_data = NULL;

void
libeventd_regex_init()
{
    GError *error = NULL;
    if ( ++regex_refcount > 1 )
        return;

    regex_event_data = g_regex_new("\\$event-data\\[([\\w-]+)\\]", G_REGEX_OPTIMIZE, 0, &error);
    if ( ! regex_event_data )
        g_warning("Can’t create $event-data regex: %s", error->message);
    g_clear_error(&error);
}

void
libeventd_regex_clean()
{
    if ( --regex_refcount > 0 )
        return;

    g_regex_unref(regex_event_data);
}

static gboolean
_libeventd_regex_event_data_cb(const GMatchInfo *info, GString *r, gpointer event_data)
{
    gchar *name;
    gchar *data = NULL;

    name = g_match_info_fetch(info, 1);
    if ( event_data != NULL )
        data = g_hash_table_lookup(event_data, name);
    g_string_append(r, data ?: "");
    g_free(name);

    return FALSE;
}

gchar *
libeventd_regex_replace_event_data(const gchar *target, GHashTable *event_data, GRegexEvalCallback callback)
{
    GError *error = NULL;
    gchar *r;

    r = g_regex_replace_eval(regex_event_data, target, -1, 0, 0, callback ?: _libeventd_regex_event_data_cb, event_data, &error);
    if ( r == NULL )
    {
        r = g_strdup(target);
        g_warning("Can’t replace event data: %s", error->message);
    }
    g_clear_error(&error);

    return r;
}
