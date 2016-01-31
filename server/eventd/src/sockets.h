/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2016 Quentin "Sardem FF7" Glidic
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

GList *eventd_sockets_get_systemd_sockets(EventdSockets *sockets);
GList *eventd_sockets_get_inet_sockets(EventdSockets *sockets, const gchar *address, guint16 port);
GList *eventd_sockets_get_inet_socket_file(EventdSockets *sockets, const gchar *file, gboolean take_over);
GList *eventd_sockets_get_unix_sockets(EventdSockets *sockets, const gchar *path, gboolean take_over);

EventdSockets *eventd_sockets_new(void);
void eventd_sockets_free(EventdSockets *sockets);

#endif /* __EVENTD_SOCKETS_H__ */
