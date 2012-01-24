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
#include <sndfile.h>

#include <glib.h>
#include <glib-object.h>

#include <libeventd-event.h>

#include "../pulseaudio.h"
#include "../pulseaudio-internal.h"
#include "sndfile-internal.h"

#include "pulseaudio.h"

static pa_context *sound = NULL;

static void
_eventd_sound_sndfile_pulseaudio_context_state_callback(pa_context *c, void *user_data)
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

typedef struct {
    gpointer data;
    gsize length;
} EventdSoundSndfilePulseaudioEventData;

static void
_eventd_sound_sndfile_pulseaudio_stream_drain_callback(pa_stream *stream, gint success, gpointer user_data)
{
    pa_stream_disconnect(stream);
}

static void
_eventd_sound_sndfile_pulseaudio_stream_state_callback(pa_stream *stream, gpointer user_data)
{
    EventdSoundSndfilePulseaudioEventData *data = user_data;
    pa_stream_state_t state = pa_stream_get_state(stream);
    pa_operation *op;
    switch ( state )
    {
        case PA_STREAM_FAILED:
            g_warning("Failed sample creation");
        case PA_STREAM_TERMINATED:
            g_free(data);
            pa_stream_unref(stream);
        break;
        case PA_STREAM_READY:
            pa_stream_write(stream, data->data, data->length, g_free, 0, PA_SEEK_RELATIVE);
            op = pa_stream_drain(stream, _eventd_sound_sndfile_pulseaudio_stream_drain_callback, NULL);
            if ( op != NULL )
                pa_operation_unref(op);
        default:
        break;
    }
}

void
eventd_sound_sndfile_pulseaudio_play_data(gpointer data, gsize length, gint format, guint32 rate, guint8 channels)
{
    pa_sample_spec sample_spec;
    pa_stream *stream;
    EventdSoundSndfilePulseaudioEventData *event_data;

    if ( data == NULL )
        return;

    if ( ( sound == NULL ) || ( pa_context_get_state(sound) != PA_CONTEXT_READY ) )
    {
        g_free(data);
        return;
    }

    switch ( format )
    {
    case SF_FORMAT_PCM_16:
    case SF_FORMAT_PCM_U8:
    case SF_FORMAT_PCM_S8:
        sample_spec.format = PA_SAMPLE_S16NE;
    break;
    case SF_FORMAT_PCM_24:
        sample_spec.format = PA_SAMPLE_S24NE;
    break;
    case SF_FORMAT_PCM_32:
        sample_spec.format = PA_SAMPLE_S32NE;
    break;
    case SF_FORMAT_FLOAT:
    case SF_FORMAT_DOUBLE:
        sample_spec.format = PA_SAMPLE_FLOAT32NE;
    break;
    default:
        g_warning("Unsupported format");
        return;
    }

    sample_spec.rate = rate;
    sample_spec.channels = channels;

    if ( ! pa_sample_spec_valid(&sample_spec) )
    {
        g_warning("Invalid spec");
        return;
    }

    stream = pa_stream_new(sound, "sndfile plugin playback", &sample_spec, NULL);

    event_data = g_new0(EventdSoundSndfilePulseaudioEventData, 1);
    event_data->data = data;
    event_data->length = length;

    pa_stream_set_state_callback(stream, _eventd_sound_sndfile_pulseaudio_stream_state_callback, event_data);
    pa_stream_connect_playback(stream, NULL, NULL, 0, NULL, NULL);
}

void
eventd_sound_sndfile_pulseaudio_start(EventdSoundPulseaudioContext *context)
{
    sound = pa_context_ref(context->sound);
    pa_context_set_state_callback(sound, _eventd_sound_sndfile_pulseaudio_context_state_callback, NULL);
}
