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

#ifndef __EVENTD_SERVICE_H__
#define __EVENTD_SERVICE_H__

GSocket *eventd_get_inet_socket(guint16 port);
#if ENABLE_GIO_UNIX
GSocket *eventd_get_unix_socket(gchar *path);
#endif /* ENABLE_GIO_UNIX */
int eventd_service(GList *sockets);

#endif /* __EVENTD_SERVICE_H__ */
