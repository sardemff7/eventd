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

#include "config.h"

#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "libeventd-event.h"
#include "libeventc.h"

#include "libeventd-helpers-reconnect.h"

#include "../eventd.h"

#include "server.h"

struct _EventdRelayServer {
    EventdCoreContext *core;
    guint ping_interval;
    GSocketConnectable *server_identity;
    gboolean accept_unknown_ca;
    GTlsCertificate *certificate;
    gboolean subscribe;
    gchar **subscriptions;
    gboolean forward_all;
    GHashTable *forwards;
    gboolean started;
    EventcConnection *connection;
    LibeventdReconnectHandler *reconnect;
    EventdEvent *current;
};

static void
_eventd_relay_server_event(EventdRelayServer *self, EventdEvent *event, EventcConnection *connection)
{
    self->current = event;
    eventd_core_push_event(self->core, event);
    self->current = NULL;
}

static void
_eventd_relay_server_disconnected(EventdRelayServer *self, EventcConnection *connection)
{
    if ( self->started )
        evhelpers_reconnect_try(self->reconnect);
}

static void
_eventd_relay_reconnect_callback(LibeventdReconnectHandler *handler, gpointer user_data)
{
    EventdRelayServer *server = user_data;

    eventd_relay_server_start(server, FALSE);
}

static void
_eventd_relay_connection_handler(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    EventdRelayServer *server = user_data;

    if ( eventc_connection_connect_finish(server->connection, res, &error) )
        evhelpers_reconnect_reset(server->reconnect);
    else if ( ! g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED) )
    {
        g_warning("Couldn't connect: %s", error->message);
        evhelpers_reconnect_try(server->reconnect);
    }
    g_clear_error(&error);
}

static void
_eventd_relay_server_setup_connection(EventdRelayServer *server)
{
    eventc_connection_set_ping_interval(server->connection, server->ping_interval);
    g_signal_connect_swapped(server->connection, "event", G_CALLBACK(_eventd_relay_server_event), server);
    g_signal_connect_swapped(server->connection, "disconnected", G_CALLBACK(_eventd_relay_server_disconnected), server);
    if ( server->server_identity != NULL )
        eventc_connection_set_server_identity(server->connection, server->server_identity);
    if ( server->subscribe )
    {
        gchar **category;
        eventc_connection_set_subscribe(server->connection, TRUE);
        if ( server->subscriptions != NULL )
        {
            for ( category = server->subscriptions ; *category != NULL ; ++category )
                eventc_connection_add_subscription(server->connection, *category);
            g_free(server->subscriptions);
            server->subscriptions = NULL;
        }
    }
}

EventdRelayServer *
eventd_relay_server_new(EventdCoreContext *core, guint ping_interval, const gchar *server_identity, gboolean accept_unknown_ca, gchar **forwards, gchar **subscriptions)
{
    EventdRelayServer *server;

    server = g_new0(EventdRelayServer, 1);
    server->core = core;

    server->ping_interval = ping_interval;
    if ( server_identity != NULL )
        server->server_identity = g_network_address_new(server_identity, 0);
    server->accept_unknown_ca = accept_unknown_ca;

    if ( forwards != NULL )
    {
        server->forward_all = ( forwards[0] == NULL );
        if ( ! server->forward_all )
        {
            server->forwards = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
            gchar **forward;
            for ( forward = forwards ; *forward != NULL ; ++forward )
                g_hash_table_add(server->forwards, *forward);
        }
        g_free(forwards);
    }

    server->subscribe = ( subscriptions != NULL );
    if ( server->subscribe && ( subscriptions[0] != NULL ) )
        server->subscriptions = subscriptions;
    else
        g_strfreev(subscriptions);

    server->reconnect = evhelpers_reconnect_new(5, 10,_eventd_relay_reconnect_callback, server);

    return server;
}

EventdRelayServer *
eventd_relay_server_new_for_uri(EventdCoreContext *core, guint ping_interval, const gchar *server_identity, gboolean accept_unknown_ca, gchar **forwards, gchar **subscriptions, const gchar *uri)
{
    EventcConnection *connection;
    GError *error = NULL;

    connection = eventc_connection_new(uri, &error);
    if ( connection == NULL )
    {
        g_warning("Couldn't get address for relay server '%s': %s", uri, error->message);
        g_clear_error(&error);
        return NULL;
    }

    EventdRelayServer *server;

    server = eventd_relay_server_new(core, ping_interval, server_identity, accept_unknown_ca, forwards, subscriptions);
    server->connection = connection;

    _eventd_relay_server_setup_connection(server);

    return server;
}

void
eventd_relay_server_set_address(EventdRelayServer *server, GSocketConnectable *address)
{
    if ( server->connection != NULL )
    {
        if ( address == NULL )
        {
            g_object_unref(server->connection);
            server->connection = NULL;
        }
        else
            eventc_connection_set_connectable(server->connection, address);
    }
    else if ( address != NULL )
    {
        server->connection = eventc_connection_new_for_connectable(address);
        _eventd_relay_server_setup_connection(server);
    }
}

gboolean
eventd_relay_server_has_address(EventdRelayServer *server)
{
    return ( server->connection != NULL );
}

gboolean
eventd_relay_server_is_connected(EventdRelayServer *server)
{
    if ( server->connection == NULL )
        return FALSE;

    GError *error = NULL;
    if ( eventc_connection_is_connected(server->connection, &error) )
        return TRUE;

    if ( error != NULL )
    {
        g_warning("Pending error: %s", error->message);
        g_clear_error(&error);
    }

    return FALSE;
}

void
eventd_relay_server_start(EventdRelayServer *server, gboolean force)
{
    if ( server->connection == NULL )
        return;

    GError *error = NULL;
    if ( eventc_connection_is_connected(server->connection, &error) )
        return;

    if ( error != NULL )
    {
        g_warning("Pending error: %s", error->message);
        g_clear_error(&error);
    }

    if ( force )
    {
        server->started = TRUE;
        evhelpers_reconnect_reset(server->reconnect);
    }
    eventc_connection_set_accept_unknown_ca(server->connection, server->accept_unknown_ca);
    eventc_connection_connect(server->connection, _eventd_relay_connection_handler, server);
}

void
eventd_relay_server_stop(EventdRelayServer *server)
{
    server->started = FALSE;
    evhelpers_reconnect_reset(server->reconnect);

    eventc_connection_close(server->connection, NULL);
}

void
eventd_relay_server_set_certificate(EventdRelayServer *server, GTlsCertificate *certificate)
{
    if ( server->certificate != NULL )
        g_object_unref(server->certificate);
    server->certificate = g_object_ref(certificate);
    if ( server->connection != NULL )
        eventc_connection_set_certificate(server->connection, server->certificate);
}

void
eventd_relay_server_event(EventdRelayServer *server, EventdEvent *event)
{
    if ( server->current == event )
        /* Do not send back events we just got */
        return;

    const gchar *category;
    category = eventd_event_get_category(event);
    if ( ( category[0] != '.' ) && ( ! server->forward_all ) && ( ( server->forwards == NULL ) || ( ! g_hash_table_contains(server->forwards, category) ) ) )
        return;

    if( ! eventd_relay_server_has_address(server) )
        return;

    GError *error = NULL;
    if ( ! eventc_connection_is_connected(server->connection, &error) )
    {
        if ( error != NULL )
        {
            g_warning("Couldn't send event: %s", error->message);
            g_clear_error(&error);
            evhelpers_reconnect_try(server->reconnect);
        }
        return;
    }

    if ( ! eventc_connection_send_event(server->connection, event, &error) )
    {
        g_warning("Couldn't send event: %s", error->message);
        g_clear_error(&error);
        evhelpers_reconnect_try(server->reconnect);
        return;
    }
}

void
eventd_relay_server_free(gpointer data)
{
    if ( data == NULL )
        return;

    EventdRelayServer *server = data;

    if ( server->connection != NULL )
        g_object_unref(server->connection);

    if ( server->certificate != NULL )
        g_object_unref(server->certificate);

    evhelpers_reconnect_free(server->reconnect);

    if ( server->forwards != NULL )
        g_hash_table_unref(server->forwards);
    g_strfreev(server->subscriptions);
    if ( server->server_identity != NULL )
        g_object_unref(server->server_identity);

    g_free(server);
}
