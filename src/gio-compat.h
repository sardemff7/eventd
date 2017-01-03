/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2017 Quentin "Sardem FF7" Glidic
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with eventd. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __EVENTD_GIO_COMPAT_H__
#define __EVENTD_GIO_COMPAT_H__

#if ! GLIB_CHECK_VERSION(2, 44, 0)
#define g_network_address_new_loopback(p) g_network_address_new("localhost", p)
#endif

#if ! GLIB_CHECK_VERSION(2, 48, 0)
#define g_socket_connectable_to_string(c) g_strdup("")
#endif

#endif /* __EVENTD_GIO_COMPAT_H__ */
