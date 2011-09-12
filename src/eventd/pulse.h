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

void eventd_pulse_start();
void eventd_pulse_stop();

int eventd_pulse_create_sample(const char *name, const char *file);
void eventd_pulse_play_sample(const char *name);
void eventd_pulse_remove_sample(const char *name);

#endif /* __EVENTD_PULSE_H__ */
