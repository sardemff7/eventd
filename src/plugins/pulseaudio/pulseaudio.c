/*
 * eventd - Small daemon to act on remote or local events
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

#include <pulse/pulseaudio.h>

#include <glib.h>

#include <eventd-plugin.h>
#include <plugins-helper.h>

#include "pulseaudio.h"

static pa_threaded_mainloop *pa_loop = NULL;
static pa_context *sound = NULL;

static GList *plugins = NULL;


static void
pa_context_state_callback(pa_context *c, void *userdata)
{
    pa_context_state_t state = pa_context_get_state(c);
    switch ( state )
    {
        case PA_CONTEXT_READY:
            pa_threaded_mainloop_signal(pa_loop, 0);
        default:
        break;
    }
}

static void
pa_context_notify_callback(pa_context *s, void *userdata)
{
    pa_threaded_mainloop_signal(pa_loop, 0);
}

static void
eventd_pulseaudio_start(gpointer user_data)
{
    EventdPulseaudioContext data;

    pa_loop = pa_threaded_mainloop_new();
    pa_threaded_mainloop_start(pa_loop);

    sound = pa_context_new(pa_threaded_mainloop_get_api(pa_loop), PACKAGE_NAME);
    if ( ! sound )
        g_error("Can't open sound system");
    pa_context_get_state(sound);
    pa_context_set_state_callback(sound, pa_context_state_callback, NULL);

    pa_threaded_mainloop_lock(pa_loop);
    pa_context_connect(sound, NULL, 0, NULL);
    pa_threaded_mainloop_wait(pa_loop);
    pa_threaded_mainloop_unlock(pa_loop);

    data.pa_loop = pa_loop;
    data.sound = sound;
    eventd_plugins_helper_load(&plugins, "plugins" G_DIR_SEPARATOR_S "pulseaudio", &data);
}

static void
eventd_pulseaudio_stop()
{
    pa_operation* op;

    eventd_plugins_helper_unload(&plugins);

    op = pa_context_drain(sound, pa_context_notify_callback, NULL);
    if ( op )
    {
        pa_threaded_mainloop_lock(pa_loop);
        pa_threaded_mainloop_wait(pa_loop);
        pa_operation_unref(op);
        pa_threaded_mainloop_unlock(pa_loop);
    }
    pa_context_disconnect(sound);
    pa_context_unref(sound);
    pa_threaded_mainloop_stop(pa_loop);
    pa_threaded_mainloop_free(pa_loop);
}

static void
eventd_pulseaudio_event_action(EventdEvent *event)
{
    eventd_plugins_helper_event_action_all(plugins, event);
}

static void
eventd_pulseaudio_event_parse(const gchar *type, const gchar *event, GKeyFile *config_file)
{
    eventd_plugins_helper_event_parse_all(plugins, type, event, config_file);
}

static void
eventd_pulseaudio_config_init()
{
    eventd_plugins_helper_config_init_all(plugins);
}

static void
eventd_pulseaudio_config_clean()
{
    eventd_plugins_helper_config_clean_all(plugins);
}

void
eventd_plugin_get_info(EventdPlugin *plugin)
{
    plugin->start = eventd_pulseaudio_start;
    plugin->stop = eventd_pulseaudio_stop;

    plugin->config_init = eventd_pulseaudio_config_init;
    plugin->config_clean = eventd_pulseaudio_config_clean;

    plugin->event_parse = eventd_pulseaudio_event_parse;
    plugin->event_action = eventd_pulseaudio_event_action;
}
