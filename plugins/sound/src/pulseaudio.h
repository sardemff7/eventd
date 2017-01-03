/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2017 Quentin "Sardem FF7" Glidic
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

#ifndef __EVENTD_PLUGINS_SOUND_PULSEAUDIO_H__
#define __EVENTD_PLUGINS_SOUND_PULSEAUDIO_H__

typedef struct _EventdSoundPulseaudioContext EventdSoundPulseaudioContext;

EventdSoundPulseaudioContext *eventd_sound_pulseaudio_init(void);
void eventd_sound_pulseaudio_uninit(EventdSoundPulseaudioContext *context);

void eventd_sound_pulseaudio_start(EventdSoundPulseaudioContext *context);
void eventd_sound_pulseaudio_stop(EventdSoundPulseaudioContext *context);

void eventd_sound_pulseaudio_play_data(EventdSoundPulseaudioContext *context, void *data, gsize length, gint format, guint32 rate, guint8 channels);

#endif /* __EVENTD_PLUGINS_SOUND_PULSEAUDIO_H__ */
