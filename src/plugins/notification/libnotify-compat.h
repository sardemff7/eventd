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

#ifndef __EVENTD_PLUGINS_NOTIFICATION_LIBNOTIFY_COMPAT_H__
#define __EVENTD_PLUGINS_NOTIFICATION_LIBNOTIFY_COMPAT_H__

#ifndef NOTIFY_CHECK_VERSION
#define NOTIFY_CHECK_VERSION(maj,min,mic) (0)
#endif /* NOTIFY_CHECK_VERSION */

#if ! NOTIFY_CHECK_VERSION(0,7,0)

#define notify_notification_new(summary, body, icon) notify_notification_new(summary, body, icon, NULL)

#endif /* !NOTIFY_CHECK_VERSION(0,7,0) */

#endif /* __EVENTD_PLUGINS_NOTIFICATION_LIBNOTIFY_COMPAT_H__ */
