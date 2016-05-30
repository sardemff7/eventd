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
#include <gio/gio.h>

#include "dns-sd.h"

#ifdef ENABLE_DNS_SD
#include <avahi-common/error.h>
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-glib/glib-watch.h>
#include <avahi-glib/glib-malloc.h>

struct _EventdEvpDNSSDContext {
    const gchar *name;
    GList *addresses;
    AvahiGLibPoll *glib_poll;
    AvahiClient *client;
    AvahiEntryGroup *group;
};

static void
_eventd_evp_dns_sd_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, void *user_data)
{
}

static void
_eventd_evp_dns_sd_create_group(EventdEvpDNSSDContext *context, AvahiClient *client)
{
    GList *address_;
    int error;

    context->group = avahi_entry_group_new(client, _eventd_evp_dns_sd_group_callback, context);
    if ( context->group == NULL )
    {
        g_warning("Couldn't create avahi entry group: %s", avahi_strerror(avahi_client_errno(client)));
        return;
    }

    for ( address_ = context->addresses ; address_ != NULL ; address_ = g_list_next(address_) )
    {
        GSocketAddress *address = address_->data;
        AvahiProtocol proto;

        switch ( g_socket_address_get_family(address) )
        {
        case G_SOCKET_FAMILY_IPV4:
            proto = AVAHI_PROTO_INET;
        break;
        case G_SOCKET_FAMILY_IPV6:
            proto = AVAHI_PROTO_INET6;
            if ( g_inet_address_get_is_any(g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(address))) )
                proto = AVAHI_PROTO_UNSPEC;
        break;
        default:
            g_return_if_reached();
        }

        if ( ( error = avahi_entry_group_add_service(context->group, AVAHI_IF_UNSPEC, proto, 0, context->name, EVP_SERVICE_TYPE, NULL, NULL, g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(address)), NULL) ) < 0 )
            g_warning("Couldn't add " EVP_SERVICE_TYPE " service: %s", avahi_strerror(error));

        g_object_unref(address);
    }
    g_list_free(context->addresses);
    context->addresses = NULL;

    if ( ! avahi_entry_group_is_empty(context->group) )
    {
        if ( ( error = avahi_entry_group_commit(context->group) ) == 0 )
            return;
        g_warning("Couldn't commit entry group: %s", avahi_strerror(error));
    }
    avahi_entry_group_free(context->group);
}

static void
_eventd_evp_dns_sd_client_callback(AvahiClient *client, AvahiClientState state, void *user_data)
{
    EventdEvpDNSSDContext *context = user_data;

    switch ( state )
    {
    case AVAHI_CLIENT_S_RUNNING:
        _eventd_evp_dns_sd_create_group(context, client);
    case AVAHI_CLIENT_S_REGISTERING:
    break;
    case AVAHI_CLIENT_FAILURE:
        avahi_client_free(client);
        context->client = NULL;
    break;
    default:
    break;
    }
}

static GList *
_eventd_evp_dns_sd_sockets_to_addresses(GList *sockets)
{
    GList *addresses = NULL;
    GError *error = NULL;
    GSocketAddress *address;

    GList *socket;
    for ( socket = sockets ; socket != NULL ; socket = g_list_next(socket) )
    {
        address = g_socket_get_local_address(socket->data, &error);
        if ( address == NULL )
        {
            g_warning("Couldn't get the socket address: %s", error->message);
            continue;
        }
        switch ( g_socket_address_get_family(address) )
        {
        case G_SOCKET_FAMILY_IPV4:
        case G_SOCKET_FAMILY_IPV6:
            if ( ! g_inet_address_get_is_loopback(g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(address))) )
            {
                addresses = g_list_prepend(addresses, address);
                break;
            }
        default:
            g_object_unref(address);
        }
    }

    return addresses;
}

EventdEvpDNSSDContext *
eventd_evp_dns_sd_start(const gchar *name, GList *sockets)
{
    EventdEvpDNSSDContext *context;
    int error;

    if ( sockets == NULL )
        return NULL;

    avahi_set_allocator(avahi_glib_allocator());

    context = g_new0(EventdEvpDNSSDContext, 1);

    context->name = name;
    context->addresses = _eventd_evp_dns_sd_sockets_to_addresses(sockets);
    context->glib_poll = avahi_glib_poll_new(NULL, G_PRIORITY_DEFAULT);
    context->client = avahi_client_new(avahi_glib_poll_get(context->glib_poll), 0, _eventd_evp_dns_sd_client_callback, context, &error);

    if ( context->client == NULL )
    {
        g_warning("Couldn't initialize Avahi: %s", avahi_strerror(error));
        eventd_evp_dns_sd_stop(context);
        return NULL;
    }

    return context;
}

void
eventd_evp_dns_sd_stop(EventdEvpDNSSDContext *context)
{
    if ( context == NULL )
        return;

    if ( context->client != NULL )
        avahi_client_free(context->client);

    avahi_glib_poll_free(context->glib_poll);

    g_list_free_full(context->addresses, g_object_unref);

    g_free(context);
}

#else /* ! ENABLE_DNS_SD */
EventdEvpDNSSDContext *eventd_evp_dns_sd_start(const gchar *name, GList *sockets) { return NULL; }
void eventd_evp_dns_sd_stop(EventdEvpDNSSDContext *context) {}
#endif /* ! ENABLE_DNS_SD */
