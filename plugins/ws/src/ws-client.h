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

#ifndef __EVENTD_WS_CLIENT_H__
#define __EVENTD_WS_CLIENT_H__

typedef struct _EventdWsClient EventdWsClient;

void evend_ws_websocket_client_handler(SoupServer *server, SoupServerMessage *server_msg, const char *path, SoupWebsocketConnection *connection, gpointer user_data);
void evend_ws_websocket_client_disconnect(gpointer data);
void evend_ws_websocket_client_event_dispatch(EventdWsClient *client, EventdEvent *event);

#endif /* __EVENTD_WS_CLIENT_H__ */
