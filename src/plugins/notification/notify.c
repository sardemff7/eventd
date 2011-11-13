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

#include <libnotify/notify.h>
#include "libnotify-compat.h"
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "notification.h"
#include "notify.h"

void
eventd_notification_notify_init()
{
    notify_init(PACKAGE_NAME);
}

void
eventd_notification_notify_uninit()
{
    notify_uninit();
}

void
eventd_notification_notify_event_action(EventdNotificationNotification *notification)
{
    GError *error = NULL;
    NotifyNotification *notify_notification = NULL;

    notify_notification = notify_notification_new(notification->title, notification->message, NULL);

    if ( notification->merged_icon != NULL )
        notify_notification_set_image_from_pixbuf(notify_notification, notification->merged_icon);
    else if ( notification->icon != NULL )
        notify_notification_set_image_from_pixbuf(notify_notification, notification->icon);
    else if ( notification->overlay_icon != NULL )
        notify_notification_set_image_from_pixbuf(notify_notification, notification->overlay_icon);

    notify_notification_set_timeout(notify_notification, notification->timeout);

    if ( ! notify_notification_show(notify_notification, &error) )
        g_warning("Can't show the notification: %s", error->message);
    g_clear_error(&error);

    g_object_unref(notify_notification);
}
