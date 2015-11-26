/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2015 Quentin "Sardem FF7" Glidic
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
#include <libeventd-event-private.h>

static struct _EventdPluginContext {
    EventdPluginCoreContext *core;
    EventdPluginCoreInterface *interface;
} _eventd_test_context;

static EventdPluginContext *
_eventd_test_init(EventdPluginCoreContext *core, EventdPluginCoreInterface *interface)
{
    _eventd_test_context.core = core;
    _eventd_test_context.interface = interface;
    return &_eventd_test_context;
}

static void
_eventd_test_uninit(EventdPluginContext *context)
{
}

static EventdPluginAction *
_eventd_test_action_parse(EventdPluginContext *context, GKeyFile *key_file)
{
    if ( ! g_key_file_has_group(key_file, "TestPlugin") )
        return NULL;

    return (EventdPluginAction *) -1;
}

static void
_eventd_test_event_action(EventdPluginContext *context, EventdPluginAction *action, EventdEvent *event)
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

    event = eventd_event_new_for_uuid_string("cedb8a77-b7fb-4e32-b3e4-3a772664f1f4", "test", "answer");
    eventd_plugin_core_push_event(context->core, context->interface, event);
    g_object_unref(event);
}

EVENTD_EXPORT const gchar *eventd_plugin_id = "test-plugin";
EVENTD_EXPORT
void
eventd_plugin_get_interface(EventdPluginInterface *interface)
{
    eventd_plugin_interface_add_init_callback(interface, _eventd_test_init);
    eventd_plugin_interface_add_uninit_callback(interface, _eventd_test_uninit);

    eventd_plugin_interface_add_action_parse_callback(interface, _eventd_test_action_parse);
    eventd_plugin_interface_add_event_action_callback(interface, _eventd_test_event_action);
}
