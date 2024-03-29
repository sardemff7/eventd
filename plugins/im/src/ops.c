/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2024 Morgane "Sardem FF7" Glidic
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

#include <glib.h>

#include <purple.h>

#include "ops.h"

#define PURPLE_GLIB_READ_COND  (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define PURPLE_GLIB_WRITE_COND (G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL)

typedef struct {
    PurpleInputFunction function;
    guint result;
    gpointer user_data;
} EventdImGLibIOData;

static void
_eventd_im_glib_io_free(gpointer data)
{
    g_slice_free(EventdImGLibIOData, data);
}

static gboolean
_eventd_im_glib_io_invoke(GIOChannel *source, GIOCondition cond, gpointer user_data)
{
    EventdImGLibIOData *data = user_data;
    PurpleInputCondition condition = 0;

    if ( cond & PURPLE_GLIB_READ_COND )
        condition |= PURPLE_INPUT_READ;
    if ( cond & PURPLE_GLIB_WRITE_COND )
        condition |= PURPLE_INPUT_WRITE;

    data->function(data->user_data, g_io_channel_unix_get_fd(source), condition);

    return TRUE;
}

guint
eventd_im_glib_input_add(gint fd, PurpleInputCondition condition, PurpleInputFunction function, gpointer user_data)
{
    EventdImGLibIOData *data;
    GIOChannel *channel;
    GIOCondition cond = 0;

    data = g_slice_new(EventdImGLibIOData);

    data->function = function;
    data->user_data = user_data;

    if ( condition & PURPLE_INPUT_READ )
        cond |= PURPLE_GLIB_READ_COND;
    if ( condition & PURPLE_INPUT_WRITE )
        cond |= PURPLE_GLIB_WRITE_COND;

    channel = g_io_channel_unix_new(fd);
    data->result = g_io_add_watch_full(channel, G_PRIORITY_DEFAULT, cond, _eventd_im_glib_io_invoke, data, _eventd_im_glib_io_free);
    g_io_channel_unref(channel);

    return data->result;
}

void
eventd_im_debug_print(PurpleDebugLevel level, const char *category, const char *message)
{
    GLogLevelFlags glib_level = G_LOG_LEVEL_DEBUG;
    switch ( level )
    {
    case PURPLE_DEBUG_ALL:
        g_return_if_reached();
    case PURPLE_DEBUG_MISC:
        glib_level = G_LOG_LEVEL_DEBUG;
    break;
    case PURPLE_DEBUG_INFO:
        glib_level = G_LOG_LEVEL_INFO;
    break;
    case PURPLE_DEBUG_WARNING:
        glib_level = G_LOG_LEVEL_WARNING;
    break;
    case PURPLE_DEBUG_ERROR:
        glib_level = G_LOG_LEVEL_CRITICAL;
    break;
    case PURPLE_DEBUG_FATAL:
        glib_level = G_LOG_LEVEL_ERROR;
    break;
    }

    gsize l = strlen("purple-") + strlen(category) + 1;
    gchar *full_category = g_newa(char, l);
    g_snprintf(full_category, l, "purple-%s", category);

    gsize ml = strlen(message);
    while ( g_ascii_isspace(message[ml-1]) )
        --ml;

    g_log_structured(full_category, glib_level, "MESSAGE", "%.*s", (int) ml, message);
}
