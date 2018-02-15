/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2018 Quentin "Sardem FF7" Glidic
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
#include <glib-object.h>

#include <pulse/pulseaudio.h>
#include <pulse/glib-mainloop.h>
#include <sndfile.h>

#include "libeventd-event.h"

#include "pulseaudio.h"

struct _EventdSoundPulseaudioContext {
    pa_context *context;
    pa_glib_mainloop *pa_loop;
};

static void
_eventd_sound_pulseaudio_context_state_callback(pa_context *c, void *user_data)
{
    pa_context_state_t state = pa_context_get_state(c);
    switch ( state )
    {
    case PA_CONTEXT_FAILED:
        g_warning("Connection to PulseAudio failed: %s", g_strerror(pa_context_errno(c)));
    case PA_CONTEXT_READY:
    case PA_CONTEXT_TERMINATED:
    default:
    break;
    }
}

static void
eventd_sound_(pa_context *s, void *user_data)
{
    EventdSoundPulseaudioContext *context = user_data;
    pa_context_disconnect(context->context);
}

typedef struct {
    gpointer data;
    gsize length;
} EventdSoundPulseaudioEventData;

static void
_eventd_sound_pulseaudio_stream_drain_callback(pa_stream *stream, gint success, gpointer user_data)
{
    pa_stream_disconnect(stream);
}

static void
_eventd_sound_pulseaudio_stream_state_callback(pa_stream *stream, gpointer user_data)
{
    EventdSoundPulseaudioEventData *data = user_data;
    pa_stream_state_t state = pa_stream_get_state(stream);
    pa_operation *op;
    switch ( state )
    {
    case PA_STREAM_FAILED:
        g_warning("Failed sample creation");
        /* fallthrough */
    case PA_STREAM_TERMINATED:
        g_free(data);
        pa_stream_unref(stream);
    break;
    case PA_STREAM_READY:
        pa_stream_write(stream, data->data, data->length, g_free, 0, PA_SEEK_RELATIVE);
        op = pa_stream_drain(stream, _eventd_sound_pulseaudio_stream_drain_callback, NULL);
        if ( op != NULL )
            pa_operation_unref(op);
    default:
    break;
    }
}

void
eventd_sound_pulseaudio_play_data(EventdSoundPulseaudioContext *context, gpointer data, gsize length, gint format, guint32 rate, guint8 channels)
{
    pa_sample_spec sample_spec;
    pa_stream *stream;
    EventdSoundPulseaudioEventData *event_data;

    if ( data == NULL )
        return;

    if ( ( context == NULL ) || ( pa_context_get_state(context->context) != PA_CONTEXT_READY ) )
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

    stream = pa_stream_new(context->context, "sndfile plugin playback", &sample_spec, NULL);

    event_data = g_new0(EventdSoundPulseaudioEventData, 1);
    event_data->data = data;
    event_data->length = length;

    pa_stream_set_state_callback(stream, _eventd_sound_pulseaudio_stream_state_callback, event_data);
    pa_stream_connect_playback(stream, NULL, NULL, 0, NULL, NULL);
}

EventdSoundPulseaudioContext *
eventd_sound_pulseaudio_init(void)
{
    EventdSoundPulseaudioContext *context;

    context = g_new0(EventdSoundPulseaudioContext, 1);

    context->pa_loop = pa_glib_mainloop_new(NULL);

    context->context = pa_context_new(pa_glib_mainloop_get_api(context->pa_loop), PACKAGE_NAME " sndfile plugin");
    if ( context->context == NULL )
    {
        g_warning("Couldn't open sound system");
        pa_glib_mainloop_free(context->pa_loop);
        g_free(context);
        return NULL;
    }

    pa_context_set_state_callback(context->context, _eventd_sound_pulseaudio_context_state_callback, NULL);

    return context;
}

void
eventd_sound_pulseaudio_uninit(EventdSoundPulseaudioContext *context)
{
    if ( context == NULL )
        return;

    pa_context_unref(context->context);
    pa_glib_mainloop_free(context->pa_loop);
    g_free(context);
}

void
eventd_sound_pulseaudio_start(EventdSoundPulseaudioContext *context)
{
    if ( context == NULL )
        return;

    pa_context_connect(context->context, NULL, 0, NULL);
}

void
eventd_sound_pulseaudio_stop(EventdSoundPulseaudioContext *context)
{
    if ( context == NULL )
        return;

    if ( context->context != NULL )
    {
        pa_operation *op;

        op = pa_context_drain(context->context, eventd_sound_, context);
        if ( op != NULL )
            pa_operation_unref(op);
    }
}
