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

#include <glib.h>

#include <eventd-plugin.h>

#include "notification.h"
#include "icon.h"

gpointer
eventd_notification_icon_get_pixbuf(const gchar *filename)
{
    return NULL;
}


void
eventd_notification_icon_get_pixbuf(EventdEvent *event, EventdNotificationEvent *notification_event, EventdNotificationNotification *notification)
{
    notification->icon = NULL;
    notification->overlay_icon = NULL;
    notification->merged_icon = NULL;
}

gchar *
eventd_notification_icon_get_base64(gpointer icon)
{
    return NULL;
}

void
eventd_notification_icon_unref(gpointer icon)
{
}
