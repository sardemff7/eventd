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

#ifndef __EVENTD_SOCKETS_H__
#define __EVENTD_SOCKETS_H__

#if ENABLE_SYSTEMD
GList *eventd_sockets_get_systemd(gchar **private_socket, gchar **unix_socket);
#endif /* ENABLE_SYSTEMD */
GList *eventd_sockets_get_all(guint16 bind_port, gboolean no_network, gboolean no_unix, gchar **private_socket, gchar **unix_socket, gboolean take_over_socket);
void eventd_sockets_free_all(GList *sockets, gchar *private_socket, gchar *unix_socket);

#endif /* __EVENTD_SOCKETS_H__ */
