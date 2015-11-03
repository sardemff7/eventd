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

#include <canberra.h>

#include <libeventd-event.h>
#include <eventd-plugin.h>
#include <libeventd-helpers-config.h>

struct _EventdPluginContext {
    ca_context *context;
    GSList *actions;
    gboolean started;
};

struct _EventdPluginAction {
    FormatString *sound_name;
    Filename *sound_file;
};


/*
 * Event contents helper
 */

static EventdPluginAction *
_eventd_canberra_event_new(FormatString *sound_name, Filename *sound_file)
{
    EventdPluginAction *event;

    event = g_slice_new(EventdPluginAction);

    event->sound_name = sound_name;
    event->sound_file = sound_file;

    return event;
}

static void
_eventd_libcanberra_action_free(gpointer data)
{
    EventdPluginAction *event = data;

    evhelpers_filename_unref(event->sound_file);
    evhelpers_format_string_unref(event->sound_name);

    g_slice_free(EventdPluginAction, event);
}


/*
 * Initialization interface
 */

static EventdPluginContext *
_eventd_libcanberra_init(EventdPluginCoreContext *core, EventdPluginCoreInterface *interface)
{
    EventdPluginContext *context;

    context = g_new0(EventdPluginContext, 1);

    int error;
    error = ca_context_create(&context->context);
    if ( error < 0 )
    {
        g_warning("Couldn't create libcanberra context: %s", ca_strerror(error));
        g_free(context);
        return NULL;
    }

    return context;
}

static void
_eventd_libcanberra_uninit(EventdPluginContext *context)
{
    ca_context_destroy(context->context);

    g_free(context);
}


/*
 * Start/Stop interface
 */

static void
_eventd_libcanberra_start(EventdPluginContext *context)
{
    int error;
    error = ca_context_open(context->context);
    if ( error < 0 )
        g_warning("Couldn't open libcanberra context: %s", ca_strerror(error));
    else
        context->started = TRUE;
}

static void
_eventd_libcanberra_stop(EventdPluginContext *context)
{
    int error;
    error = ca_context_cancel(context->context, 1);
    if ( error < 0 )
        g_warning("Couldn't cancel sounds: %s", ca_strerror(error));
    context->started = FALSE;
}


/*
 * Configuration interface
 */

static void
_eventd_libcanberra_global_parse(EventdPluginContext *context, GKeyFile *config_file)
{
    if ( ! g_key_file_has_group(config_file, "Libcanberra") )
        return;

    gchar * sound_theme = NULL;

    if ( evhelpers_config_key_file_get_string(config_file, "Libcanberra", "Theme", &sound_theme) < 0 )
        goto skip;

    /*
     * TODO: handle that
     */

skip:
    g_free(sound_theme);
}

static EventdPluginAction *
_eventd_libcanberra_action_parse(EventdPluginContext *context, GKeyFile *config_file)
{
    gboolean disable;
    FormatString *sound_name = NULL;
    Filename *sound_file = NULL;

    if ( ! g_key_file_has_group(config_file, "Libcanberra") )
        return NULL;

    if ( evhelpers_config_key_file_get_boolean(config_file, "Libcanberra", "Disable", &disable) < 0 )
        return NULL;

    if ( disable )
        return NULL;

    if ( evhelpers_config_key_file_get_format_string_with_default(config_file, "Libcanberra", "Name", "${sound-name}", &sound_name) < 0 )
        goto fail;
#ifndef ENABLE_SOUND
    if ( evhelpers_config_key_file_get_filename_with_default(config_file, "Libcanberra", "File", "sound-file", &sound_file) < 0 )
        goto fail;
#endif /* ! ENABLE_SOUND */

    EventdPluginAction *action;
    action = _eventd_canberra_event_new(sound_name, sound_file);
    sound_file = NULL;
    sound_name = NULL;

    context->actions = g_slist_prepend(context->actions, action);

    return action;

fail:
    evhelpers_filename_unref(sound_file);
    evhelpers_format_string_unref(sound_name);
    return NULL;
}

static void
_eventd_libcanberra_config_reset(EventdPluginContext *context)
{
    g_slist_free_full(context->actions, _eventd_libcanberra_action_free);
    context->actions = NULL;
}


/*
 * Event action interface
 */

static void
_eventd_libcanberra_event_action(EventdPluginContext *context, EventdPluginAction *action, EventdEvent *event)
{
    int error;

    gchar *sound_name;
    sound_name = evhelpers_format_string_get_string(action->sound_name, event, NULL, NULL);
    error = ca_context_play(context->context, 1,
        CA_PROP_EVENT_ID, sound_name,
        CA_PROP_MEDIA_ROLE, "event",
        NULL);
    if ( error < 0 )
        g_warning("Couldn't play named sound '%s': %s", sound_name, ca_strerror(error));
    g_free(sound_name);

#ifndef ENABLE_SOUND
    gchar *sound_file;
    if ( evhelpers_filename_get_path(action->sound_file, event, "sounds", NULL, &sound_file) )
    {
        if ( sound_file != NULL )
        {
            error = ca_context_play(context->context, 1,
                CA_PROP_MEDIA_FILENAME, sound_file,
                CA_PROP_MEDIA_ROLE, "event",
                NULL);
            if ( error < 0 )
                g_warning("Couldn't play sound file '%s': %s", sound_file, ca_strerror(error));
        }
        g_free(sound_file);
    }
#endif /* ! ENABLE_SOUND */
}


/*
 * Plugin interface
 */

EVENTD_EXPORT const gchar *eventd_plugin_id = "canberra";
EVENTD_EXPORT
void
eventd_plugin_get_interface(EventdPluginInterface *interface)
{
    eventd_plugin_interface_add_init_callback(interface, _eventd_libcanberra_init);
    eventd_plugin_interface_add_uninit_callback(interface, _eventd_libcanberra_uninit);

    eventd_plugin_interface_add_start_callback(interface, _eventd_libcanberra_start);
    eventd_plugin_interface_add_stop_callback(interface, _eventd_libcanberra_stop);

    eventd_plugin_interface_add_global_parse_callback(interface, _eventd_libcanberra_global_parse);
    eventd_plugin_interface_add_action_parse_callback(interface, _eventd_libcanberra_action_parse);
    eventd_plugin_interface_add_config_reset_callback(interface, _eventd_libcanberra_config_reset);

    eventd_plugin_interface_add_event_action_callback(interface, _eventd_libcanberra_event_action);
}

