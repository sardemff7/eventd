/*
 * eventd - Small daemon to act on remote or local events
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

#include <config.h>

#include <glib.h>
#include <glib-object.h>

#include <eventd-plugin.h>
#include <libeventd-event.h>

static gboolean
_eventd_test_event_end_earlier(gpointer user_data)
{
    EventdEvent *event = user_data;

    eventd_event_answer(event, "test");
    eventd_event_end(event, EVENTD_EVENT_END_REASON_RESERVED);
    g_object_unref(event);

    return FALSE;
}

static void
_eventd_test_event_action(EventdPluginContext *context, const gchar *config_id, EventdEvent *event)
{
    GError *error = NULL;

    const gchar *filename;
    const gchar *contents;

    filename = eventd_event_get_data(event, "file");
    contents = eventd_event_get_data(event, "test");

    if ( ( filename == NULL ) || ( contents == NULL ) )
        return;

    if ( ! g_file_set_contents(filename, contents, -1, &error) )
        g_warning("Couldn’t write to file: %s", error->message);
    g_clear_error(&error);

    eventd_event_add_answer_data(event, "test", g_strdup(contents));

    g_idle_add(_eventd_test_event_end_earlier, g_object_ref(event));
}

EVENTD_EXPORT const gchar *eventd_plugin_id = "test-plugin";
EVENTD_EXPORT
void
eventd_plugin_get_interface(EventdPluginInterface *interface)
{
    eventd_plugin_interface_add_event_action_callback(interface, _eventd_test_event_action);
}
