/*
 * libeventc-light - Library to communicate with eventd, light (local-only non-GIO) version
 *
 * Copyright © 2011-2021 Quentin "Sardem FF7" Glidic
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

#ifndef __EVENTC_LIGHT_CONNECTION_H__
#define __EVENTC_LIGHT_CONNECTION_H__

#include <glib.h>
#include "libeventd-event.h"

G_BEGIN_DECLS

typedef struct _EventcLightConnection EventcLightConnection;

#ifdef G_OS_UNIX
typedef gint EventcLightSocket;
#else /* ! G_OS_UNIX */
#include <winsock2.h>
#include <windows.h>
typedef SOCKET EventcLightSocket;
#endif /* ! G_OS_UNIX */

typedef void (*EventcLightConnectionReceivedEventCallback)(EventcLightConnection *connection, EventdEvent *event, gpointer user_data);
typedef void (*EventcLightConnectionDisconnectedCallback)(EventcLightConnection *connection, gpointer user_data);

const gchar *eventc_light_get_version(void);


EventcLightConnection *eventc_light_connection_new(const gchar *name);
EventcLightConnection *eventc_light_connection_ref(EventcLightConnection *connection);
void eventc_light_connection_unref(EventcLightConnection *connection);

void eventc_light_connection_set_received_event_callback(EventcLightConnection *connection, EventcLightConnectionReceivedEventCallback callback, gpointer data, GDestroyNotify notify);
void eventc_light_connection_set_disconnected_callback(EventcLightConnection *connection, EventcLightConnectionDisconnectedCallback callback, gpointer data, GDestroyNotify notify);
EventcLightSocket eventc_light_connection_get_socket(EventcLightConnection *connection);
gint eventc_light_connection_read(EventcLightConnection *connection);

gint eventc_light_connection_connect(EventcLightConnection *connection);
gint eventc_light_connection_send_event(EventcLightConnection *connection, EventdEvent *event);
gint eventc_light_connection_close(EventcLightConnection *connection);

gboolean eventc_light_connection_is_connected(EventcLightConnection *connection, gint *error);

void eventc_light_connection_set_name(EventcLightConnection *connection, const gchar *name);
void eventc_light_connection_set_subscribe(EventcLightConnection *connection, gboolean subscribe);
void eventc_light_connection_add_subscription(EventcLightConnection *connection, gchar *category);

gboolean eventc_light_connection_get_subscribe(EventcLightConnection *connection);

G_END_DECLS

#endif /* __EVENTC_LIGHT_CONNECTION_H__ */
