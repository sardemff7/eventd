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

#include <glib.h>

#include "pulseaudio.h"
#include "pulseaudio-internal.h"

static void
_eventd_sound_pulseaudio_context_state_callback(pa_context *c, void *user_data)
{
    EventdSoundPulseaudioContext *context = user_data;
    pa_context_state_t state = pa_context_get_state(c);
    switch ( state )
    {
        case PA_CONTEXT_FAILED:
            pa_context_unref(context->sound);
            context->sound = NULL;
        case PA_CONTEXT_READY:
        break;
        case PA_CONTEXT_TERMINATED:
            pa_context_unref(context->sound);
            context->sound = NULL;
            pa_glib_mainloop_free(context->pa_loop);
            g_free(context);
        default:
        break;
    }
}

static void
_eventd_sound_pulseaudio_context_notify_callback(pa_context *s, void *user_data)
{
    EventdSoundPulseaudioContext *context = user_data;
    pa_context_disconnect(context->sound);
}

EventdSoundPulseaudioContext *
eventd_sound_pulseaudio_start()
{
    EventdSoundPulseaudioContext *context;

    context = g_new0(EventdSoundPulseaudioContext, 1);

    context->pa_loop = pa_glib_mainloop_new(NULL);

    context->sound = pa_context_new(pa_glib_mainloop_get_api(context->pa_loop), PACKAGE_NAME);
    if ( context->sound == NULL )
    {
        g_warning("Can't open sound system");
        pa_glib_mainloop_free(context->pa_loop);
        g_free(context);
        return NULL;
    }

    pa_context_set_state_callback(context->sound, _eventd_sound_pulseaudio_context_state_callback, context);
    pa_context_connect(context->sound, NULL, 0, NULL);

    return context;
}

void
eventd_sound_pulseaudio_stop(EventdSoundPulseaudioContext *context)
{
    if ( context == NULL )
        return;

    if ( context->sound != NULL )
    {
        pa_operation* op;

        op = pa_context_drain(context->sound, _eventd_sound_pulseaudio_context_notify_callback, context);
        if ( op != NULL )
            pa_operation_unref(op);
    }
    else
    {
        pa_glib_mainloop_free(context->pa_loop);
        g_free(context);
    }
}
