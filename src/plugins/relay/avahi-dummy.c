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

#include <glib.h>

#include "types.h"
#include "avahi.h"

EventdRelayAvahi *eventd_relay_avahi_init(void) { return NULL; }
void eventd_relay_avahi_uninit(EventdRelayAvahi *context) {}

EventdRelayAvahiServer *eventd_relay_avahi_server_new(EventdRelayAvahi *context, const gchar *name, EventdRelayServer *relay_server) { return NULL; }
void eventd_relay_avahi_server_free(EventdRelayAvahiServer *server) {}
