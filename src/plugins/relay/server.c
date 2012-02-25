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
#include <glib-object.h>
#include <gio/gio.h>

#include <libeventd-event.h>
#include <eventd-plugin.h>
#include <libeventc.h>

#include "types.h"

#include "avahi.h"

#include "server.h"

struct _EventdRelayServer {
    EventdRelayAvahiServer *avahi;
    EventcConnection *connection;
    gboolean avahi_connected;
    guint64 tries;
    guint connection_timeout_id;
};

static void
_eventd_relay_server_create_eventc(EventdRelayServer *server, const gchar *host, guint16 port)
{
    server->connection = eventc_connection_new(host, port, PACKAGE_NAME);
    server->connection->mode = EVENTC_CONNECTION_MODE_RELAY;
}

void
eventd_relay_server_avahi_connect(EventdRelayServer *server, const gchar *host, guint16 port)
{
    if ( server->avahi_connected )
        return;
    server->avahi_connected = TRUE;

    _eventd_relay_server_create_eventc(server, host, port);
    eventd_relay_server_start(server);
}

EventdRelayServer *
eventd_relay_server_new(const gchar *uri)
{
    EventdRelayServer *server;
    gchar *colon;
    gchar *host;
    guint16 port = DEFAULT_BIND_PORT;

    if ( ( colon = g_utf8_strrchr(uri, -1, ':') ) != NULL )
    {
        port = g_ascii_strtoull(colon+1, NULL, 10);
        host = g_strndup(uri, colon-uri);
    }
    else
        host = g_strdup(uri);

    if ( g_str_has_prefix(host, "[") && g_str_has_suffix(host, "]") )
    {
        gchar *tmp = host;
        host = g_utf8_substring(tmp, 1, g_utf8_strlen(tmp, -1));
        g_free(tmp);
    }

    server = g_new0(EventdRelayServer, 1);

    _eventd_relay_server_create_eventc(server, host, port);

    g_free(host);

    return server;
}

EventdRelayServer *
eventd_relay_server_new_avahi(EventdRelayAvahi *context, const gchar *name)
{
    EventdRelayServer *server;

    server = g_new0(EventdRelayServer, 1);

    server->avahi = eventd_relay_avahi_server_new(context, name, server);

    if ( server->avahi == NULL )
    {
        g_free(server);
        return NULL;
    }

    return server;
}

static gboolean
_eventd_relay_reconnect(gpointer user_data)
{
    EventdRelayServer *server = user_data;

    server->connection_timeout_id = 0;
    eventd_relay_server_start(server);

    return FALSE;
}

static void
_eventd_relay_connection_handler(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    EventdRelayServer *server = user_data;

    eventc_connection_connect_finish(EVENTC_CONNECTION(obj), res, &error);
    if ( error != NULL )
    {
        g_warning("Couldn’t connect: %s", error->message);
        g_clear_error(&error);
        if ( ++server->tries < 4 )
            server->connection_timeout_id = g_timeout_add_seconds(3, _eventd_relay_reconnect, server);
    }
}

void
eventd_relay_server_start(EventdRelayServer *server)
{
    if ( server->connection == NULL )
        return;

    eventc_connection_connect(server->connection, _eventd_relay_connection_handler, server);
}

static void
_eventd_relay_close_handler(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;

    eventc_connection_close_finish(EVENTC_CONNECTION(obj), res, &error);
    if ( error != NULL )
    {
        g_warning("Couldn’t close connection: %s", error->message);
        g_clear_error(&error);
    }
}

void
eventd_relay_server_stop(EventdRelayServer *server)
{
    if ( server->connection == NULL )
        return;

    if ( ! server->avahi_connected )
        return;
    server->avahi_connected = FALSE;

    if ( server->connection_timeout_id > 0 )
    {
        g_source_remove(server->connection_timeout_id);
        server->connection_timeout_id = 0;
    }

    if ( eventc_connection_is_connected(server->connection) )
        eventc_connection_close(server->connection, _eventd_relay_close_handler, NULL);
}

static void
_eventd_relay_event_handler(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    EventdRelayServer *server = user_data;

    eventc_connection_event_finish(EVENTC_CONNECTION(obj), res, &error);
    if ( error != NULL )
    {
        g_warning("Couldn’t send event: %s", error->message);
        g_clear_error(&error);
        server->connection_timeout_id = g_timeout_add_seconds(1, _eventd_relay_reconnect, server);
    }
}

void
eventd_relay_server_event(EventdRelayServer *server, EventdEvent *event)
{
    if ( ( server->connection == NULL ) || ( ! eventc_connection_is_connected(server->connection) ) )
        return;

    eventc_connection_event(server->connection, event, _eventd_relay_event_handler, server);
}

void
eventd_relay_server_free(gpointer data)
{
    EventdRelayServer *server = data;

    if ( server->connection_timeout_id > 0 )
        g_source_remove(server->connection_timeout_id);

    if ( server->connection != NULL )
        g_object_unref(server->connection);

    if ( server->avahi != NULL )
        eventd_relay_avahi_server_free(server->avahi);

    g_free(server);
}
