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
_eventd_sound_sndfile_sample_state_callback(pa_stream *sample, void *user_data)
{
    EventdSoundSndfileEvent *event = user_data;
    pa_stream_state_t state = pa_stream_get_state(sample);
    switch ( state )
    {
        case PA_STREAM_TERMINATED:
            pa_stream_unref(sample);
        break;
        case PA_STREAM_READY:
            pa_stream_write(sample, event->data, event->length, NULL, 0, PA_SEEK_RELATIVE);
            pa_stream_finish_upload(sample);
        default:
        break;
    }
}

void
eventd_sound_sndfile_pulseaudio_create_sample(EventdSoundSndfileEvent *event, SF_INFO *sfi)
{
    pa_sample_spec sample_spec;
    pa_stream *sample;

    switch ( sfi->format & SF_FORMAT_SUBMASK )
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

    sample_spec.rate = (uint32_t)sfi->samplerate;
    sample_spec.channels = (uint8_t)sfi->channels;

    if ( ! pa_sample_spec_valid(&sample_spec) )
    {
        g_warning("Invalid spec");
        return;
    }

    sample = pa_stream_new(sound, event->sample, &sample_spec, NULL);

    pa_stream_set_state_callback(sample, _eventd_sound_sndfile_sample_state_callback, event);
    pa_stream_connect_upload(sample, event->length);
}

void
eventd_sound_sndfile_pulseaudio_play_sample(const gchar *name)
{
    pa_operation *op;

    op = pa_context_play_sample(sound, name, NULL, PA_VOLUME_INVALID, NULL, NULL);
    if ( op )
        pa_operation_unref(op);
    else
        g_warning("Can't play sample %s", name);
}

void
eventd_sound_sndfile_pulseaudio_remove_sample(const char *name)
{
    pa_context_remove_sample(sound, name, NULL, NULL);
}

void
eventd_sound_sndfile_pulseaudio_start(EventdSoundPulseaudioContext *context)
{
    sound = context->sound;
}
