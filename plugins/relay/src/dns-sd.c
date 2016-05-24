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

#include <config.h>

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <libeventd-event.h>
#include <eventd-plugin.h>

#include "server.h"

#include "dns-sd.h"

#ifdef ENABLE_DNS_SD
#include <avahi-common/error.h>
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-glib/glib-watch.h>
#include <avahi-glib/glib-malloc.h>

struct _EventdRelayDNSSD {
    AvahiGLibPoll *glib_poll;
    AvahiClient *client;
    AvahiServiceBrowser *browser;
    GHashTable *servers;
};

static void
_eventd_relay_dns_sd_service_resolve_callback(AvahiServiceResolver *r, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event, const gchar *name, const gchar *type, const gchar *domain, const gchar *host_name, const AvahiAddress *address, guint16 port, AvahiStringList *txt, AvahiLookupResultFlags flags, void *user_data)
{
    EventdRelayServer *server = user_data;

    switch ( event )
    {
    case AVAHI_RESOLVER_FAILURE:
        g_warning("Service '%s', resolver failure: %s", name, avahi_strerror(avahi_client_errno(avahi_service_resolver_get_client(r))));
    break;
    case AVAHI_RESOLVER_FOUND:
#ifdef EVENTD_DEBUG
        g_debug("Service '%s' resolved: [%s]:%" G_GUINT16_FORMAT, name, host_name, port);
#endif /* EVENTD_DEBUG */
        if ( ! eventd_relay_server_has_address(server) )
        {
            eventd_relay_server_set_address(server, g_network_address_new(host_name, port));
            eventd_relay_server_start(server);
        }
    default:
    break;
    }

    avahi_service_resolver_free(r);
}

static void
_eventd_relay_dns_sd_service_browser_callback(AvahiServiceBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const gchar *name, const gchar *type, const gchar *domain, AvahiLookupResultFlags flags, void *user_data)
{
    EventdRelayDNSSD *context = user_data;
    EventdRelayServer *server;

    switch ( event )
    {
    case AVAHI_BROWSER_FAILURE:
        g_warning("Avahi Browser failure: %s", avahi_strerror(avahi_client_errno(context->client)));
        avahi_service_browser_free(context->browser);
        context->browser = NULL;
    break;
    case AVAHI_BROWSER_NEW:
#ifdef EVENTD_DEBUG
        g_debug("Service found in '%s' domain: %s", domain, name);
#endif /* EVENTD_DEBUG */
        if ( ( server = g_hash_table_lookup(context->servers, name) ) != NULL )
            avahi_service_resolver_new(context->client, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, 0, _eventd_relay_dns_sd_service_resolve_callback, server);
    break;
    case AVAHI_BROWSER_REMOVE:
#ifdef EVENTD_DEBUG
        g_debug("Service removed in '%s' domain: %s", domain, name);
#endif /* EVENTD_DEBUG */
        if ( ( ( server = g_hash_table_lookup(context->servers, name) ) != NULL ) && ( eventd_relay_server_has_address(server) ) )
        {
            eventd_relay_server_stop(server);
            eventd_relay_server_set_address(server, NULL);
        }
    break;
    case AVAHI_BROWSER_ALL_FOR_NOW:
    case AVAHI_BROWSER_CACHE_EXHAUSTED:
    break;
    }
}


static void
_eventd_relay_dns_sd_client_callback(AvahiClient *client, AvahiClientState state, void *user_data)
{
    EventdRelayDNSSD *context = user_data;

    switch ( state )
    {
    case AVAHI_CLIENT_S_RUNNING:
        context->browser = avahi_service_browser_new(client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, EVP_SERVICE_TYPE, NULL, 0, _eventd_relay_dns_sd_service_browser_callback, context);
    case AVAHI_CLIENT_S_REGISTERING:
    break;
    case AVAHI_CLIENT_FAILURE:
        avahi_client_free(context->client);
        context->client = NULL;
    break;
    default:
    break;
    }
}


EventdRelayDNSSD *
eventd_relay_dns_sd_init(void)
{
    EventdRelayDNSSD *context;

    avahi_set_allocator(avahi_glib_allocator());

    context = g_new0(EventdRelayDNSSD, 1);

    context->glib_poll = avahi_glib_poll_new(NULL, G_PRIORITY_DEFAULT);

    context->servers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    return context;
}

void
eventd_relay_dns_sd_uninit(EventdRelayDNSSD *context)
{
    if ( context == NULL )
        return;

    g_hash_table_unref(context->servers);

    avahi_glib_poll_free(context->glib_poll);

    g_free(context);
}

void
eventd_relay_dns_sd_start(EventdRelayDNSSD *context)
{
    if ( context == NULL )
        return;

    int error;

    context->client = avahi_client_new(avahi_glib_poll_get(context->glib_poll), 0, _eventd_relay_dns_sd_client_callback, context, &error);
    if ( context->client == NULL )
        g_warning("Couldn't initialize Avahi: %s", avahi_strerror(error));
}

void
eventd_relay_dns_sd_stop(EventdRelayDNSSD *context)
{
    if ( ( context == NULL ) || ( context->client == NULL ) )
        return;

    if ( context->browser != NULL )
        avahi_service_browser_free(context->browser);

    avahi_client_free(context->client);
}


void
eventd_relay_dns_sd_monitor_server(EventdRelayDNSSD *context, const gchar *name, EventdRelayServer *server)
{
    if ( context == NULL )
        return;

    g_hash_table_insert(context->servers, g_strdup(name), server);
}

#else /* ! ENABLE_DNS_SD */
EventdRelayDNSSD *eventd_relay_dns_sd_init(void) { return NULL; }
void eventd_relay_dns_sd_uninit(EventdRelayDNSSD *context) {}

void eventd_relay_dns_sd_start(EventdRelayDNSSD *context) {}
void eventd_relay_dns_sd_stop(EventdRelayDNSSD *context) {}

void eventd_relay_dns_sd_monitor_server(EventdRelayDNSSD *context, const gchar *name, EventdRelayServer *relay_server) {}
#endif /* ! ENABLE_DNS_SD */
