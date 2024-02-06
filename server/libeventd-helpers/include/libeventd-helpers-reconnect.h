/*
 * libeventd - Internal helper
 *
 * Copyright © 2011-2024 Morgane "Sardem FF7" Glidic
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

#ifndef __LIBEVENTD_RECONNECT_H__
#define __LIBEVENTD_RECONNECT_H__

typedef struct _LibeventdReconnectHandler LibeventdReconnectHandler;

typedef void (*LibeventdReconnectTryCallback)(LibeventdReconnectHandler *handler, gpointer user_data);

LibeventdReconnectHandler *evhelpers_reconnect_new(gint64 timeout, gint64 max_tries, LibeventdReconnectTryCallback callback, gpointer user_data);
void evhelpers_reconnect_free(LibeventdReconnectHandler *handler);

gboolean evhelpers_reconnect_too_much(LibeventdReconnectHandler *self);
gboolean evhelpers_reconnect_try(LibeventdReconnectHandler *handler);
void evhelpers_reconnect_reset(LibeventdReconnectHandler *handler);

#endif /* __LIBEVENTD_RECONNECT_H__ */
