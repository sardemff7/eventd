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
#include <gio/gio.h>
#include <avahi-common/error.h>
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-glib/glib-watch.h>
#include <avahi-glib/glib-malloc.h>

#include "avahi.h"

struct _EventdEvpAvahiContext {
    const gchar *name;
    GList *sockets;
    AvahiGLibPoll *glib_poll;
    AvahiClient *client;
    AvahiEntryGroup *group;
};

static void
_eventd_evp_avahi_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, void *user_data)
{
}

static void
_eventd_evp_avahi_create_group(EventdEvpAvahiContext *context, AvahiClient *client)
{
    GList *socket;
    int error;

    context->group = avahi_entry_group_new(client, _eventd_evp_avahi_group_callback, context);
    if ( context->group == NULL )
    {
        g_warning("Couldn't create avahi entry group: %s", avahi_strerror(avahi_client_errno(client)));
        return;
    }

    for ( socket = context->sockets ; socket != NULL ; socket = g_list_next(socket) )
    {
        AvahiProtocol proto;
        GError *g_error = NULL;
        GSocketAddress *address;

        address = g_socket_get_local_address(socket->data, &g_error);
        if ( address == NULL )
        {
            g_warning("Couldn't get the socket address: %s", g_error->message);
            continue;
        }
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
            goto next;
        }

        if ( ( error = avahi_entry_group_add_service(context->group, AVAHI_IF_UNSPEC, proto, 0, context->name, "_event._tcp", NULL, NULL, g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(address)), NULL) ) < 0 )
        {
            g_warning("Couldn't add _event._tcp service: %s", avahi_strerror(error));
            goto fail;
        }

    next:
        g_object_unref(address);
    }
    g_list_free_full(context->sockets, g_object_unref);
    context->sockets = NULL;

    if ( ( error = avahi_entry_group_commit(context->group) ) < 0 )
    {
        g_warning("Couldn't commit entry group: %s", avahi_strerror(error));
        goto fail;
    }

    return;

fail:
    avahi_entry_group_free(context->group);
}

static void
_eventd_evp_avahi_client_callback(AvahiClient *client, AvahiClientState state, void *user_data)
{
    EventdEvpAvahiContext *context = user_data;

    switch ( state )
    {
    case AVAHI_CLIENT_S_RUNNING:
        _eventd_evp_avahi_create_group(context, client);
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

EventdEvpAvahiContext *
eventd_evp_avahi_start(const gchar *name, GList *sockets)
{
    EventdEvpAvahiContext *context;
    int error;

    if ( sockets == NULL )
        return NULL;

    avahi_set_allocator(avahi_glib_allocator());

    context = g_new0(EventdEvpAvahiContext, 1);

    context->name = name;
    context->sockets = sockets;
    context->glib_poll = avahi_glib_poll_new(NULL, G_PRIORITY_DEFAULT);
    context->client = avahi_client_new(avahi_glib_poll_get(context->glib_poll), 0, _eventd_evp_avahi_client_callback, context, &error);

    if ( context->client == NULL )
    {
        g_warning("Couldn't initialize Avahi: %s", avahi_strerror(error));
        eventd_evp_avahi_stop(context);
        return NULL;
    }

    return context;
}

void
eventd_evp_avahi_stop(EventdEvpAvahiContext *context)
{
    if ( context == NULL )
        return;

    if ( context->client != NULL )
        avahi_client_free(context->client);

    avahi_glib_poll_free(context->glib_poll);

    g_list_free_full(context->sockets, g_object_unref);

    g_free(context);
}
