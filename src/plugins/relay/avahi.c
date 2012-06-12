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
#include <avahi-common/error.h>
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-glib/glib-watch.h>
#include <avahi-glib/glib-malloc.h>

#include <libeventd-event.h>

#include "types.h"

#include "server.h"

#include "avahi.h"

struct _EventdRelayAvahi {
    AvahiGLibPoll *glib_poll;
    AvahiClient *client;
    AvahiServiceBrowser *browser;
    GHashTable *servers;
};

struct _EventdRelayAvahiServer {
    EventdRelayAvahi *context;
    gchar *name;
    EventdRelayServer *server;
};


static void
_eventd_relay_avahi_client_callback(AvahiClient *client, AvahiClientState state, void *user_data)
{
    EventdRelayAvahi *context = user_data;

    switch ( state )
    {
    case AVAHI_CLIENT_S_REGISTERING:
    case AVAHI_CLIENT_S_RUNNING:
    break;
    case AVAHI_CLIENT_FAILURE:
        avahi_client_free(context->client);
        context->client = NULL;
    break;
    default:
    break;
    }
}


static void
_eventd_relay_avahi_service_resolve_callback(AvahiServiceResolver *r, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event, const gchar *name, const gchar *type, const gchar *domain, const gchar *host_name, const AvahiAddress *address, guint16 port, AvahiStringList *txt, AvahiLookupResultFlags flags, void *user_data)
{
    EventdRelayAvahiServer *server = user_data;

    switch ( event )
    {
    case AVAHI_RESOLVER_FAILURE:
    break;
    case AVAHI_RESOLVER_FOUND:
        eventd_relay_server_avahi_connect(server->server, host_name, port);
    default:
    break;
    }

    avahi_service_resolver_free(r);
}

static void
_eventd_relay_avahi_service_browser_callback(AvahiServiceBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const gchar *name, const gchar *type, const gchar *domain, AvahiLookupResultFlags flags, void *user_data)
{
    EventdRelayAvahi *context = user_data;
    EventdRelayAvahiServer *server;

    switch ( event )
    {
    case AVAHI_BROWSER_FAILURE:
        avahi_service_browser_free(context->browser);
        context->browser = NULL;
    break;
    case AVAHI_BROWSER_NEW:
        if ( ( server = g_hash_table_lookup(context->servers, name) ) != NULL )
            avahi_service_resolver_new(context->client, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, 0, _eventd_relay_avahi_service_resolve_callback, server);
    break;
    case AVAHI_BROWSER_REMOVE:
        if ( ( server = g_hash_table_lookup(context->servers, name) ) != NULL )
            eventd_relay_server_stop(server->server);
    break;
    case AVAHI_BROWSER_ALL_FOR_NOW:
    case AVAHI_BROWSER_CACHE_EXHAUSTED:
    break;
    }
}

EventdRelayAvahi *
eventd_relay_avahi_init()
{
    EventdRelayAvahi *context;
    int error;

    avahi_set_allocator(avahi_glib_allocator());

    context = g_new0(EventdRelayAvahi, 1);

    context->glib_poll = avahi_glib_poll_new(NULL, G_PRIORITY_DEFAULT);
    context->client = avahi_client_new(avahi_glib_poll_get(context->glib_poll), 0, _eventd_relay_avahi_client_callback, context, &error);

    if ( context->client == NULL )
    {
        g_warning("Couldn’t initialize Avahi: %s", g_strerror(error));
        eventd_relay_avahi_uninit(context);
        return NULL;
    }

    context->browser = avahi_service_browser_new(context->client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, "_event._tcp", NULL, 0, _eventd_relay_avahi_service_browser_callback, context);

    context->servers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    return context;
}

void
eventd_relay_avahi_uninit(EventdRelayAvahi *context)
{
    if ( context == NULL )
        return;

    g_hash_table_unref(context->servers);

    if ( context->browser != NULL )
        avahi_service_browser_free(context->browser);

    if ( context->client != NULL )
        avahi_client_free(context->client);

    avahi_glib_poll_free(context->glib_poll);

    g_free(context);
}


EventdRelayAvahiServer *
eventd_relay_avahi_server_new(EventdRelayAvahi *context, const gchar *name, EventdRelayServer *relay_server)
{
    EventdRelayAvahiServer *server;

    server = g_new0(EventdRelayAvahiServer, 1);

    server->name = g_strdup(name);
    server->context = context;
    server->server = relay_server;

    g_hash_table_insert(server->context->servers, server->name, server);

    return server;
}

void
eventd_relay_avahi_server_free(EventdRelayAvahiServer *server)
{
    g_hash_table_remove(server->context->servers, server->name);

    g_free(server);
}
