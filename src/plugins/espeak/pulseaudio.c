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

#include "espeak.h"
#include "pulseaudio.h"

struct _EventdEspeakPulseaudioContext {
    pa_context *context;
    pa_glib_mainloop *pa_loop;
    pa_sample_spec sample_spec;
};


#define BUFFER_SIZE 2000

static void
_eventd_espeak_pulseaudio_context_state_callback(pa_context *c, void *user_data)
{
    pa_context_state_t state = pa_context_get_state(c);
    switch ( state )
    {
    case PA_CONTEXT_FAILED:
        g_warning("Connection to PulseAudio failed: %s", pa_strerror(pa_context_errno(c)));
    case PA_CONTEXT_READY:
    case PA_CONTEXT_TERMINATED:
    default:
    break;
    }
}

static void
_eventd_espeak_pulseaudio_stream_state_callback(pa_stream *stream, void *userdata)
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

static void
_eventd_espeak_pulseaudio_context_notify_callback(pa_context *s, void *user_data)
{
    EventdEspeakPulseaudioContext *context = user_data;
    pa_context_disconnect(context->context);
}

EventdEspeakPulseaudioContext *
eventd_espeak_pulseaudio_init(gint sample_rate)
{
    EventdEspeakPulseaudioContext *context;

    context = g_new0(EventdEspeakPulseaudioContext, 1);

    context->pa_loop = pa_glib_mainloop_new(NULL);

    context->sample_spec.rate = sample_rate;
    context->sample_spec.channels = 1;
    context->sample_spec.format = PA_SAMPLE_S16LE;

    context->context = pa_context_new(pa_glib_mainloop_get_api(context->pa_loop), PACKAGE_NAME " eSpeak plugin");
    if ( context->context == NULL )
    {
        g_warning("Couldn’t open sound system");
        pa_glib_mainloop_free(context->pa_loop);
        g_free(context);
        return NULL;
    }

    pa_context_set_state_callback(context->context, _eventd_espeak_pulseaudio_context_state_callback, NULL);

    return context;
}

void
eventd_espeak_pulseaudio_uninit(EventdEspeakPulseaudioContext *context)
{
    if ( context == NULL )
        return;

    pa_context_unref(context->context);
    pa_glib_mainloop_free(context->pa_loop);
    g_free(context);
}

void
eventd_espeak_pulseaudio_start(EventdEspeakPulseaudioContext *context)
{
    if ( context == NULL )
        return;

    pa_context_connect(context->context, NULL, 0, NULL);
}

void
eventd_espeak_pulseaudio_stop(EventdEspeakPulseaudioContext *context)
{
    if ( context == NULL )
        return;

    if ( context->context != NULL )
    {
        pa_operation *op;

        op = pa_context_drain(context->context, _eventd_espeak_pulseaudio_context_notify_callback, context);
        if ( op != NULL )
            pa_operation_unref(op);
    }
}

void
eventd_espeak_pulseaudio_play_data(gshort *wav, gint numsamples, espeak_EVENT *event)
{
    pa_stream *stream;
    EventdEspeakCallbackData *callback_data;

    callback_data = event->user_data;

    stream = callback_data->data;

    if ( ( stream == NULL ) || ( pa_stream_get_state(stream) != PA_STREAM_READY ) )
        return;

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
eventd_espeak_pulseaudio_pa_data_new(EventdEspeakPulseaudioContext *context)
{
    pa_stream *stream;

    if ( ( context == NULL ) || ( pa_context_get_state(context->context) != PA_CONTEXT_READY ) )
        return NULL;

    stream = pa_stream_new(context->context, "eSpeak eventd message", &context->sample_spec, NULL);
    pa_stream_set_state_callback(stream, _eventd_espeak_pulseaudio_stream_state_callback, context);
    pa_stream_connect_playback(stream, NULL, NULL, 0, NULL, NULL);

    return stream;
}

void
eventd_espeak_pulseaudio_pa_data_free(gpointer stream)
{
    if ( stream == NULL )
        return;

    pa_stream_disconnect(stream);
}
