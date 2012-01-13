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

#include <libnotify/notify.h>
#include "libnotify-compat.h"
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <glib.h>
#include <glib-object.h>

#include "icon.h"
#include "notification.h"
#include "notify.h"

static GdkPixbuf *
_eventd_notification_notify_get_icon(EventdNotificationNotification *notification, gdouble overlay_scale)
{
    GdkPixbuf *icon;
    GdkPixbuf *overlay_icon;

    icon = eventd_notification_icon_get_pixbuf_from_data(notification->icon, notification->icon_length, 0);
    overlay_icon = eventd_notification_icon_get_pixbuf_from_data(notification->overlay_icon, notification->overlay_icon_length, 0);

    if ( ( icon != NULL ) && ( overlay_icon != NULL ) )
    {
        gint icon_width, icon_height;
        gint overlay_icon_width, overlay_icon_height;
        gint x, y;
        gdouble scale;

        icon_width = gdk_pixbuf_get_width(icon);
        icon_height = gdk_pixbuf_get_height(icon);

        overlay_icon_width = overlay_scale * (gdouble)icon_width;
        overlay_icon_height = overlay_scale * (gdouble)icon_height;

        x = icon_width - overlay_icon_width;
        y = icon_height - overlay_icon_height;

        scale = (gdouble)overlay_icon_width / (gdouble)gdk_pixbuf_get_width(overlay_icon);

        gdk_pixbuf_composite(overlay_icon, icon,
                             x, y,
                             overlay_icon_width, overlay_icon_height,
                             x, y,
                             scale, scale,
                             GDK_INTERP_BILINEAR, 255);

        g_object_unref(overlay_icon);
    }
    else if ( ( icon == NULL ) && ( overlay_icon != NULL ) )
        icon = overlay_icon;

    return icon;
}

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
eventd_notification_notify_event_action(EventdNotificationNotification *notification, guint64 timeout, gdouble scale)
{
    GError *error = NULL;
    NotifyNotification *notify_notification = NULL;
    GdkPixbuf *icon;

    notify_notification = notify_notification_new(notification->title, notification->message, NULL);

    icon = _eventd_notification_notify_get_icon(notification, scale);
    if ( icon != NULL )
    {
        notify_notification_set_image_from_pixbuf(notify_notification, icon);
        g_object_unref(icon);
    }

    notify_notification_set_timeout(notify_notification, timeout);

    if ( ! notify_notification_show(notify_notification, &error) )
        g_warning("Can't show the notification: %s", error->message);
    g_clear_error(&error);

    g_object_unref(notify_notification);
}
