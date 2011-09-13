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


void
eventd_notify_start()
{
	notify_init(PACKAGE_NAME);
}

void
eventd_notify_stop()
{
	notify_uninit();
}

static gboolean
notification_closed_cb(NotifyNotification *notification)
{
	return FALSE;
}

void
eventd_notify_display(const char *title, const char *msg)
{
	GError *error = NULL;
	NotifyNotification *notification = notify_notification_new(title, msg, NULL
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
}
