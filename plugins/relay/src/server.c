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

#include <config.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <libeventd-event.h>
#include <eventd-plugin.h>
#include <libeventc.h>

#include <libeventd-helpers-reconnect.h>

#include "server.h"

struct _EventdPluginAction {
    EventdPluginCoreContext *core;
    EventdPluginCoreInterface *core_interface;
    gboolean subscribe;
    gchar **subscriptions;
    EventcConnection *connection;
    LibeventdReconnectHandler *reconnect;
};

static gboolean
_eventd_relay_server_discard_event(gpointer data)
{
    EventdEvent *event = data;

    eventd_event_end(event, EVENTD_EVENT_END_REASON_DISCARD);

    return FALSE;
}
static void
_eventd_relay_server_event(EventdRelayServer *self, EventdEvent *event, EventcConnection *connection)
{
    if ( ! eventd_plugin_core_push_event(self->core, self->core_interface, event) )
        g_idle_add(_eventd_relay_server_discard_event, event);
}

static void
_eventd_relay_reconnect_callback(LibeventdReconnectHandler *handler, gpointer user_data)
{
    EventdRelayServer *server = user_data;

    eventd_relay_server_start(server);
}

static void
_eventd_relay_connection_handler(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    EventdRelayServer *server = user_data;

    if ( eventc_connection_connect_finish(server->connection, res, &error) )
        evhelpers_reconnect_reset(server->reconnect);
    else
    {
        g_warning("Couldn't connect: %s", error->message);
        g_clear_error(&error);
        evhelpers_reconnect_try(server->reconnect);
    }
}

static void
_eventd_relay_server_setup_connection(EventdRelayServer *server)
{
    g_signal_connect_swapped(server->connection, "event", G_CALLBACK(_eventd_relay_server_event), server);
    if ( server->subscribe )
    {
        gchar **category;
        eventc_connection_set_subscribe(server->connection, TRUE);
        for ( category = server->subscriptions ; *category != NULL ; ++category )
            eventc_connection_add_subscription(server->connection, *category);
        g_free(server->subscriptions);
        server->subscriptions = NULL;
    }
}

EventdRelayServer *
eventd_relay_server_new(EventdPluginCoreContext *core, EventdPluginCoreInterface *core_interface, gboolean subscribe, gchar **subscriptions)
{
    EventdRelayServer *server;

    server = g_new0(EventdRelayServer, 1);
    server->core = core;
    server->core_interface = core_interface;

    server->subscribe = subscribe;
    if ( server->subscribe && ( subscriptions != NULL ) && ( subscriptions[0] != NULL ) )
        server->subscriptions = subscriptions;
    else
        g_strfreev(subscriptions);

    server->reconnect = evhelpers_reconnect_new(5, 10,_eventd_relay_reconnect_callback, server);

    return server;
}

EventdRelayServer *
eventd_relay_server_new_for_domain(EventdPluginCoreContext *core, EventdPluginCoreInterface *core_interface, gboolean subscribe, gchar **subscriptions, const gchar *domain)
{
    EventcConnection *connection;
    GError *error = NULL;

    connection = eventc_connection_new(domain, &error);
    if ( connection == NULL )
    {
        g_warning("Couldn't get address for relay server '%s': %s", domain, error->message);
        g_clear_error(&error);
        return NULL;
    }

    EventdRelayServer *server;

    server = eventd_relay_server_new(core, core_interface, subscribe, subscriptions);
    server->connection = connection;

    _eventd_relay_server_setup_connection(server);

    return server;
}

void
eventd_relay_server_set_address(EventdRelayServer *server, GSocketConnectable *address)
{
    if ( server->connection != NULL )
        eventc_connection_set_connectable(server->connection, address);
    else
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

void
eventd_relay_server_start(EventdRelayServer *server)
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

    eventc_connection_connect(server->connection, _eventd_relay_connection_handler, server);
}

void
eventd_relay_server_stop(EventdRelayServer *server)
{
    evhelpers_reconnect_reset(server->reconnect);

    eventc_connection_close(server->connection, NULL);
}

void
eventd_relay_server_event(EventdRelayServer *server, EventdEvent *event)
{
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

    if ( ! eventc_connection_event(server->connection, event, &error) )
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

    evhelpers_reconnect_free(server->reconnect);

    g_free(server);
}
