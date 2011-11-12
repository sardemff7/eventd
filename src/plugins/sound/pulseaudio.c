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
#include <libeventd-plugins.h>

#include "pulseaudio.h"
#include "pulseaudio-internal.h"

static pa_threaded_mainloop *pa_loop = NULL;
static pa_context *sound = NULL;

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

EventdSoundPulseaudioContext *
eventd_sound_pulseaudio_start()
{
    EventdSoundPulseaudioContext *data;

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

    data = g_new0(EventdSoundPulseaudioContext, 1);
    data->pa_loop = pa_loop;
    data->sound = sound;

    return data;
}

void
eventd_sound_pulseaudio_stop()
{
    pa_operation* op;

    op = pa_context_drain(sound, pa_context_notify_callback, NULL);
    if ( op != NULL )
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
