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
#include <speak_lib.h>

#include <string.h>
#include <glib.h>
#include <gio/gio.h>

#include <eventd-plugin.h>
#include <plugin-helper.h>

#include "../pulseaudio.h"
#include "../pulseaudio-internal.h"

#include "espeak.h"
#include "pulseaudio.h"

static pa_threaded_mainloop *pa_loop = NULL;
static pa_context *sound = NULL;

static pa_sample_spec sample_spec;

#define BUFFER_SIZE 2000

static void
pa_stream_state_callback(pa_stream *stream, void *userdata)
{
    pa_stream_state_t state = pa_stream_get_state(stream);
    switch  ( state )
    {
        case PA_STREAM_TERMINATED:
            //pa_stream_unref(stream);
        break;
        case PA_STREAM_READY:
            pa_threaded_mainloop_signal(pa_loop, 0);
        default:
        break;
    }
}

static void
pa_stream_drain_callback(pa_stream *stream, int success, void *userdata)
{
    pa_threaded_mainloop_signal(pa_loop, 0);
}

void
eventd_sound_espeak_pulseaudio_start(EventdSoundPulseaudioContext *context, gint sample_rate)
{
    sample_spec.rate = sample_rate;
    sample_spec.channels = 1;
    sample_spec.format = PA_SAMPLE_S16LE;

    pa_loop = context->pa_loop;
    sound = context->sound;
}

void
eventd_sound_espeak_pulseaudio_play_data(gshort *wav, gint numsamples, espeak_EVENT *event)
{
    pa_stream *stream;
    EventdSoundEspeakCallbackData *callback_data;

    callback_data = event->user_data;

    stream = callback_data->data;

    pa_threaded_mainloop_lock(pa_loop);

    if ( wav == NULL )
    {
        pa_operation *op;

        op = pa_stream_drain(stream, pa_stream_drain_callback, NULL);
        while ( pa_operation_get_state(op) == PA_OPERATION_RUNNING )
            pa_threaded_mainloop_wait(pa_loop);
        pa_operation_unref(op);
    }
    else
        pa_stream_write(stream, wav, numsamples*2, NULL, 0, PA_SEEK_RELATIVE);

    pa_threaded_mainloop_unlock(pa_loop);
}

gpointer
eventd_sound_espeak_pulseaudio_pa_data_new()
{
    pa_stream *stream;

    stream = pa_stream_new(sound, "eSpeak eventd message", &sample_spec, NULL);
    pa_stream_set_state_callback(stream, pa_stream_state_callback, NULL);
    pa_stream_connect_playback(stream, NULL, NULL, 0, NULL, NULL);

    return stream;
}

void
eventd_sound_espeak_pulseaudio_pa_data_free(gpointer stream)
{
    pa_threaded_mainloop_lock(pa_loop);
    pa_threaded_mainloop_wait(pa_loop);

    pa_stream_disconnect(stream);

    pa_threaded_mainloop_unlock(pa_loop);
}
