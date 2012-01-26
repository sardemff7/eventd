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

#include <pulse/pulseaudio.h>
#include <pulse/glib-mainloop.h>
#include <speak_lib.h>

#include <string.h>
#include <glib.h>
#include <gio/gio.h>

#include <libeventd-regex.h>

#include "../pulseaudio.h"
#include "../pulseaudio-internal.h"

#include "espeak.h"
#include "pulseaudio.h"

static pa_context *sound = NULL;

static pa_sample_spec sample_spec;

#define BUFFER_SIZE 2000

static void
_eventd_sound_espeak_pulseaudio_context_state_callback(pa_context *c, void *user_data)
{
    pa_context_state_t state = pa_context_get_state(c);
    switch ( state )
    {
    case PA_CONTEXT_READY:
    break;
    case PA_CONTEXT_FAILED:
    case PA_CONTEXT_TERMINATED:
        pa_context_unref(sound);
        sound = NULL;
    default:
    break;
    }
}

static void
_eventd_sound_espeak_pulseaudio_stream_state_callback(pa_stream *stream, void *userdata)
{
    pa_stream_state_t state = pa_stream_get_state(stream);
    switch  ( state )
    {
    case PA_STREAM_TERMINATED:
        pa_stream_unref(stream);
    break;
    case PA_STREAM_READY:
    default:
    break;
    }
}

void
eventd_sound_espeak_pulseaudio_start(EventdSoundPulseaudioContext *context, gint sample_rate)
{
    sample_spec.rate = sample_rate;
    sample_spec.channels = 1;
    sample_spec.format = PA_SAMPLE_S16LE;

    sound = pa_context_ref(context->sound);
    pa_context_set_state_callback(sound, _eventd_sound_espeak_pulseaudio_context_state_callback, NULL);
}

void
eventd_sound_espeak_pulseaudio_play_data(gshort *wav, gint numsamples, espeak_EVENT *event)
{
    pa_stream *stream;
    EventdSoundEspeakCallbackData *callback_data;

    callback_data = event->user_data;

    stream = callback_data->data;

    if ( wav == NULL )
    {
        pa_operation *op;

        op = pa_stream_drain(stream, NULL, NULL);
        if ( op != NULL )
            pa_operation_unref(op);
    }
    else
        pa_stream_write(stream, wav, numsamples*sizeof(gshort), NULL, 0, PA_SEEK_RELATIVE);
}

gpointer
eventd_sound_espeak_pulseaudio_pa_data_new()
{
    pa_stream *stream;

    stream = pa_stream_new(sound, "eSpeak eventd message", &sample_spec, NULL);
    pa_stream_set_state_callback(stream, _eventd_sound_espeak_pulseaudio_stream_state_callback, NULL);
    pa_stream_connect_playback(stream, NULL, NULL, 0, NULL, NULL);

    return stream;
}

void
eventd_sound_espeak_pulseaudio_pa_data_free(gpointer stream)
{
    pa_stream_disconnect(stream);
}
