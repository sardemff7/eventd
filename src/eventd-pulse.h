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

#ifndef __EVENTD_PULSE_H__
#define __EVENTD_PULSE_H__

#include <pulse/pulseaudio.h>

void pa_context_state_callback(pa_context *c, void *userdata);
void pa_context_notify_callback(pa_context *s, void *userdata);
void pa_context_success_callback(pa_context *s, int success, void *userdata);
void pa_sample_state_callback(pa_stream *sample, void *userdata);

extern pa_threaded_mainloop *pa_loop;
extern pa_context *sound;

#endif /* __EVENTD_PULSE_H__ */
