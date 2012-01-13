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

#ifndef __EVENTD_PLUGINS_SOUND_ESPEAK_PULSEAUDIO_H__
#define __EVENTD_PLUGINS_SOUND_ESPEAK_PULSEAUDIO_H__

void eventd_sound_espeak_pulseaudio_start(EventdSoundPulseaudioContext *context, gint sample_rate);

void eventd_sound_espeak_pulseaudio_play_data(gshort *wav, gint numsamples, espeak_EVENT *event);

gpointer eventd_sound_espeak_pulseaudio_pa_data_new();
void eventd_sound_espeak_pulseaudio_pa_data_free(gpointer pa_data);

#endif /* __EVENTD_PLUGINS_SOUND_ESPEAK_PULSEAUDIO_H__ */
