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

#include "eventd.h"

#include <libnotify/notify.h>

#include <glib.h>

#include "notify.h"

static GRegex *client_name_regex = NULL;
static GRegex *event_name_regex = NULL;
static GRegex *event_data_regex = NULL;

void
eventd_notify_start()
{
	GError *error = NULL;

	notify_init(PACKAGE_NAME);

	client_name_regex = g_regex_new("\\$client-name", G_REGEX_OPTIMIZE, 0, &error);
	if ( ! client_name_regex )
		g_warning("Can’t create $client-name regex: %s", error->message);
	g_clear_error(&error);

	event_name_regex = g_regex_new("\\$event-name", G_REGEX_OPTIMIZE, 0, &error);
	if ( ! event_name_regex )
		g_warning("Can’t create $event-name regex: %s", error->message);
	g_clear_error(&error);

	event_data_regex = g_regex_new("\\$event-data", G_REGEX_OPTIMIZE, 0, &error);
	if ( ! event_data_regex )
		g_warning("Can’t create $event-data regex: %s", error->message);
	g_clear_error(&error);
}

void
eventd_notify_stop()
{
	g_regex_unref(event_data_regex);
	g_regex_unref(event_name_regex);
	g_regex_unref(client_name_regex);
	notify_uninit();
}

static gboolean
notification_closed_cb(NotifyNotification *notification)
{
	return FALSE;
}

EventdNotifyEvent *
eventd_notify_event_new(const char *title, const char *message)
{
	EventdNotifyEvent *event = NULL;

	event = g_new0(EventdNotifyEvent, 1);

	event->title = g_strdup(title ? title : "$client-name - $event-name");
	event->message = g_strdup(message ? message : "$event-data");

	return event;
}

void
eventd_notify_event_perform(EventdNotifyEvent *event, const gchar *client_name, const gchar *event_name, const gchar *event_data)
{
	GError *error = NULL;
	gchar *title = NULL;
	gchar *message = NULL;
	gchar *tmp = NULL;
	NotifyNotification *notification = NULL;

	tmp = g_regex_replace_literal(client_name_regex, event->title, -1, 0, client_name ?: "" , 0, &error);
	if ( ! tmp )
		g_warning("Can’t replace client name: %s", error->message);
	g_clear_error(&error);
	title = g_regex_replace_literal(event_name_regex, tmp, -1, 0, event_name ?: "", 0, &error);
	if ( ! title )
		g_warning("Can’t replace event name: %s", error->message);
	g_clear_error(&error);
	g_free(tmp);

	message = g_regex_replace_literal(event_data_regex, event->message, -1, 0, event_data ?: "", 0, &error);
	if ( ! message )
		g_warning("Can’t replace event data: %s", error->message);
	g_clear_error(&error);

	notification = notify_notification_new(title, message, NULL
	#if ! NOTIFY_CHECK_VERSION(0,7,0)
	, NULL
	#endif
	);
	notify_notification_set_urgency(notification, NOTIFY_URGENCY_NORMAL);
	notify_notification_set_timeout(notification, 1);
	if ( ! notify_notification_show(notification, &error) )
		g_warning("Can't show the notification: %s", error->message);
	g_clear_error(&error);
	g_signal_connect(notification, "closed", G_CALLBACK(notification_closed_cb), NULL);
	g_object_unref(G_OBJECT(notification));
	g_free(message);
	g_free(title);
}

void
eventd_notify_event_free(EventdNotifyEvent *event)
{
	g_free(event->message);
	g_free(event->title);
	g_free(event);
}
