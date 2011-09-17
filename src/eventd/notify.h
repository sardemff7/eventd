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

#ifndef __EVENTD_NOTIFY_H__
#define __EVENTD_NOTIFY_H__

void eventd_notify_start();
void eventd_notify_stop();

typedef struct {
    gchar *title;
    gchar *message;
} EventdNotifyEvent;

EventdNotifyEvent *eventd_notify_event_new(const char *title, const char *message);
void eventd_notify_event_perform(EventdNotifyEvent *event, const gchar *client_name, const gchar *event_name, const gchar *event_data);
void eventd_notify_event_free(EventdNotifyEvent *event);

void eventd_notify_display(const char *title, const char *msg);

#endif /* __EVENTD_NOTIFY_H__ */
