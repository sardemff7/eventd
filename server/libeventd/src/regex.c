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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <glib.h>
#include <glib-object.h>

#include <libeventd-event.h>

#include <libeventd-regex.h>

static guint64 regex_refcount = 0;

static GRegex *regex_event_data = NULL;

EVENTD_EXPORT
void
libeventd_regex_init(void)
{
    GError *error = NULL;
    if ( ++regex_refcount > 1 )
        return;

    regex_event_data = g_regex_new("\\$(\\{)?([A-Za-z0-9-]+)(?(1)\\})", G_REGEX_OPTIMIZE, 0, &error);
    if ( ! regex_event_data )
        g_warning("Couldn't create $event-data regex: %s", error->message);
    g_clear_error(&error);
}

EVENTD_EXPORT
void
libeventd_regex_clean(void)
{
    if ( --regex_refcount > 0 )
        return;

    g_regex_unref(regex_event_data);
}

typedef struct {
    EventdEvent *event;
    LibeventdRegexReplaceCallback callback;
    gpointer user_data;
} LibeventdRegexReplaceData;

static gchar *
_libeventd_regex_replace_cb(const gchar *name, EventdEvent *event)
{
    const gchar *data;

    data = eventd_event_get_data(event, name);

    return g_strdup(( data != NULL ) ? data : "");
}

static gboolean
_libeventd_regex_replace_event_data_cb(const GMatchInfo *info, GString *r, gpointer user_data)
{
    gchar *data = NULL;
    gchar *name;
    LibeventdRegexReplaceData *replace_data = user_data;

    name = g_match_info_fetch(info, 2);
    if ( replace_data->callback != NULL )
        data = replace_data->callback(name, replace_data->event, replace_data->user_data);
    else
        data = _libeventd_regex_replace_cb(name, replace_data->event);
    g_free(name);

    if ( data == NULL )
        return TRUE;

    g_string_append(r, data);
    g_free(data);

    return FALSE;
}

EVENTD_EXPORT
gchar *
libeventd_regex_replace_event_data(const gchar *target, EventdEvent *event, LibeventdRegexReplaceCallback callback, gpointer user_data)
{
    GError *error = NULL;
    gchar *r;
    LibeventdRegexReplaceData data;

    data.event = event;
    data.callback = callback;
    data.user_data = user_data;

    r = g_regex_replace_eval(regex_event_data, target, -1, 0, 0, _libeventd_regex_replace_event_data_cb, &data, &error);
    if ( r == NULL )
    {
        r = g_strdup(target);
        g_warning("Couldn't replace event data: %s", error->message);
    }
    g_clear_error(&error);

    return r;
}
