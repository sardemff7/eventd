/*
 * Eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011 Sardem FF7
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "eventd-pulse.h"

pa_threaded_mainloop *pa_loop = NULL;
pa_context *sound = NULL;

void
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

void
pa_context_notify_callback(pa_context *s, void *userdata)
{
	pa_threaded_mainloop_signal(pa_loop, 0);
}

void
pa_context_success_callback(pa_context *s, int success, void *userdata)
{
	pa_threaded_mainloop_signal(pa_loop, 0);
}

void
pa_sample_state_callback(pa_stream *sample, void *userdata)
{
	pa_stream_state_t state = pa_stream_get_state(sample);
	switch  ( state )
	{
		case PA_STREAM_TERMINATED:
			pa_stream_unref(sample);
		break;
		case PA_STREAM_READY:
			pa_threaded_mainloop_signal(pa_loop, 0);
		default:
		break;
	}
}
