/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2017 Quentin "Sardem FF7" Glidic
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

#ifndef __EVENTD_PLUGINS_RELAY_SERVER_H__
#define __EVENTD_PLUGINS_RELAY_SERVER_H__

#include "../types.h"
#include "eventd-sd-module.h"

EventdRelayServer *eventd_relay_server_new(EventdCoreContext *core, const gchar *server_identity, gboolean accept_unknown_ca, gboolean use_websocket, gchar **forwards, gchar **subscriptions);
EventdRelayServer *eventd_relay_server_new_for_domain(EventdCoreContext *core, const gchar *server_identity, gboolean accept_unknown_ca, gboolean use_websocket, gchar **forwards, gchar **subscriptions, const gchar *domain);
void eventd_relay_server_free(gpointer data);

void eventd_relay_server_set_address(EventdRelayServer *server, GSocketConnectable *address);
gboolean eventd_relay_server_has_address(EventdRelayServer *server);

gboolean eventd_relay_server_is_connected(EventdRelayServer *server);

void eventd_relay_server_start(EventdRelayServer *server, gboolean force);
void eventd_relay_server_stop(EventdRelayServer *server);

void eventd_relay_server_set_certificate(EventdRelayServer *server, GTlsCertificate *certificate);

void eventd_relay_server_event(EventdRelayServer *server, EventdEvent *event);

#endif /* __EVENTD_PLUGINS_RELAY_SERVER_H__ */
