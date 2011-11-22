/*
 * libeventd - Internal helper
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

#ifndef __LIBEVENTD_CLIENT_H__
#define __LIBEVENTD_CLIENT_H__

#include <libeventd-types.h>

EventdClient *libeventd_client_new();
EventdClient *libeventd_client_ref(EventdClient *client);
void libeventd_client_unref(EventdClient *client);

void libeventd_client_update(EventdClient *client, const gchar *type, const gchar *name);
void libeventd_client_set_mode(EventdClient *client, EventdClientMode mode);

const gchar * libeventd_client_get_type(EventdClient *client);
const gchar * libeventd_client_get_name(EventdClient *client);
EventdClientMode libeventd_client_get_mode(EventdClient *client);

#endif /* __LIBEVENTD_CLIENT_H__ */
