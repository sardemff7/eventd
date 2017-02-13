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

#ifndef __EVENTD_WS_MODULE_H__
#define __EVENTD_WS_MODULE_H__

#include "libeventd-event.h"
#include "libeventd-protocol.h"

typedef struct _EventdWsConnection EventdWsConnection;

typedef struct {
    EventdWsConnection *(*connection_server_new)(gpointer data, GDestroyNotify disconnect_callback, GCancellable *cancellable, GIOStream *stream, GDataInputStream *input, EventdProtocol *protocol, const gchar *line);
    EventdWsConnection *(*connection_client_new)(gpointer data, GDestroyNotify disconnect_callback, GCancellable *cancellable, GIOStream *stream, EventdProtocol *protocol);
    void (*connection_free)(EventdWsConnection *connection);

    void (*connection_client_connect)(EventdWsConnection *connection, GSocketConnectable *server_identity, GAsyncReadyCallback callback, gpointer user_data);
    gboolean (*connection_client_connect_finish)(EventdWsConnection *connection, GAsyncResult *result, GError **error);
    gboolean (*connection_client_connect_sync)(EventdWsConnection *connection, GSocketConnectable *server_identity, GError **error);

    void (*connection_send_message)(EventdWsConnection *client, const gchar *message);
    void (*connection_close)(EventdWsConnection *connection);

    gpointer module;
} EventdWsModule;

typedef void (*EventdWsModuleGetInfoFunc)(EventdWsModule *backend);
void eventd_ws_module_get_info(EventdWsModule *backend);


EventdWsModule *eventd_ws_init(void);
void eventd_ws_uninit(EventdWsModule *ws);

static inline EventdWsConnection *
eventd_ws_connection_server_new(EventdWsModule *ws, gpointer client, GDestroyNotify disconnect_callback, GCancellable *cancellable, GIOStream *stream, GDataInputStream *input, EventdProtocol *protocol, const gchar *line)
{
    g_return_val_if_fail(ws != NULL, NULL);
    return ws->connection_server_new(client, disconnect_callback, cancellable, stream, input, protocol, line);
}

static inline EventdWsConnection *
eventd_ws_connection_client_new(EventdWsModule *ws, gpointer client, GDestroyNotify disconnect_callback, GCancellable *cancellable, GIOStream *stream, EventdProtocol *protocol)
{
    g_return_val_if_fail(ws != NULL, NULL);
    return ws->connection_client_new(client, disconnect_callback, cancellable, stream, protocol);
}

static inline void
eventd_ws_connection_client_connect(EventdWsModule *ws, EventdWsConnection *connection, GSocketConnectable *server_identity, GAsyncReadyCallback callback, gpointer user_data)
{
    g_return_if_fail(ws != NULL);
    ws->connection_client_connect(connection, server_identity, callback, user_data);
}

static inline gboolean
eventd_ws_connection_client_connect_finish(EventdWsModule *ws, EventdWsConnection *connection, GAsyncResult *result, GError **error)
{
    g_return_val_if_fail(ws != NULL, FALSE);
    return ws->connection_client_connect_finish(connection, result, error);
}

static inline gboolean
eventd_ws_connection_client_connect_sync(EventdWsModule *ws, EventdWsConnection *connection, GSocketConnectable *server_identity, GError **error)
{
    g_return_val_if_fail(ws != NULL, FALSE);
    return ws->connection_client_connect_sync(connection, server_identity, error);
}

static inline void
eventd_ws_connection_free(EventdWsModule *ws, EventdWsConnection *connection)
{
    g_return_if_fail(ws != NULL);
    ws->connection_free(connection);
}

static inline void
eventd_ws_connection_send_message(EventdWsModule *ws, EventdWsConnection *connection, const gchar *message)
{
    g_return_if_fail(ws != NULL);
    ws->connection_send_message(connection, message);
}

static inline void
eventd_ws_connection_close(EventdWsModule *ws, EventdWsConnection *connection)
{
    g_return_if_fail(ws != NULL);
    ws->connection_close(connection);
}

#endif /* __EVENTD_WS_MODULE_H__ */
