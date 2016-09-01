/*
 * libeventc - Library to communicate with eventd
 *
 * Copyright © 2011-2016 Quentin "Sardem FF7" Glidic
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

#ifndef __EVENTC_CONNECTION_H__
#define __EVENTC_CONNECTION_H__

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include "libeventd-event.h"

G_BEGIN_DECLS

typedef struct _EventcConnection EventcConnection;
typedef struct _EventcConnectionClass EventcConnectionClass;
typedef struct _EventcConnectionPrivate EventcConnectionPrivate;

typedef enum  {
	EVENTC_ERROR_HOSTNAME,
	EVENTC_ERROR_CONNECTION,
	EVENTC_ERROR_ALREADY_CONNECTED,
	EVENTC_ERROR_NOT_CONNECTED,
	EVENTC_ERROR_RECEIVE,
	EVENTC_ERROR_EVENT,
	EVENTC_ERROR_END,
	EVENTC_ERROR_BYE
} EventcError;


GType eventc_connection_get_type(void) G_GNUC_CONST;
GQuark eventc_error_quark(void);

#define EVENTC_TYPE_CONNECTION            (eventc_connection_get_type())
#define EVENTC_CONNECTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), EVENTC_TYPE_CONNECTION, EventcConnection))
#define EVENTC_IS_CONNECTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), EVENTC_TYPE_CONNECTION))
#define EVENTC_CONNECTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), EVENTC_TYPE_CONNECTION, EventcConnectionClass))
#define EVENTC_IS_CONNECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), EVENTC_TYPE_CONNECTION))
#define EVENTC_CONNECTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EVENTC_TYPE_CONNECTION, EventcConnectionClass))

#define EVENTC_ERROR                      (eventc_error_quark())


struct _EventcConnection {
	GObject parent_instance;

	/*< private >*/
	EventcConnectionPrivate  *priv;
};

struct _EventcConnectionClass {
	GObjectClass parent_class;

	/* Signals */
	void (*event)(EventcConnection *connection, EventdEvent *event);
};


const gchar *eventc_get_version(void);


EventcConnection *eventc_connection_new(const gchar *host, GError **error);
EventcConnection *eventc_connection_new_for_connectable(GSocketConnectable *address);

void eventc_connection_connect(EventcConnection *connection, GAsyncReadyCallback callback, gpointer user_data);
gboolean eventc_connection_connect_finish(EventcConnection *connection, GAsyncResult *result, GError **error);
gboolean eventc_connection_connect_sync(EventcConnection *connection, GError **error);
gboolean eventc_connection_event (EventcConnection *connection, EventdEvent *event, GError **error);
gboolean eventc_connection_close(EventcConnection *connection, GError **error);


gboolean eventc_connection_set_use_websocket(EventcConnection *connection, gboolean use_websocket, GError **error);
gboolean eventc_connection_set_host(EventcConnection *connection, const gchar *host, GError **error);
void eventc_connection_set_connectable(EventcConnection *connection, GSocketConnectable *address);
void eventc_connection_set_server_identity(EventcConnection *connection, GSocketConnectable *server_identity);
void eventc_connection_set_accept_unknown_ca(EventcConnection *connection, gboolean accept_unknown_ca);
void eventc_connection_set_subscribe(EventcConnection *connection, gboolean subscribe);
void eventc_connection_add_subscription(EventcConnection *connection, gchar *category);

gboolean eventc_connection_is_connected(EventcConnection *connection, GError **error);
gboolean eventc_connection_get_subscribe(EventcConnection *connection);

G_END_DECLS

#endif /* __EVENTC_CONNECTION_H__ */
