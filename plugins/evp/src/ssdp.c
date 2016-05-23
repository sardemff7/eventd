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
#include <nkutils-uuid.h>

#include "ssdp.h"

#ifdef ENABLE_SSDP
#include <libgssdp/gssdp.h>

struct _EventdEvpSSDPContext {
    GSSDPClient *client;
    GSSDPResourceGroup *group;
};

static GList *
_eventd_evp_ssdp_sockets_to_locations(EventdEvpSSDPContext *self, GList *sockets)
{
    GList *locations = NULL;

    GList *socket;
    for ( socket = sockets ; socket != NULL ; socket = g_list_next(socket) )
    {
        GError *error = NULL;
        GSocketAddress *address;

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
        {
            GInetAddress *inet_address;
            gchar *ip_ = NULL;
            const gchar *ip = NULL;

            inet_address = g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(address));
            if ( g_inet_address_get_is_any(inet_address) )
                ip = gssdp_client_get_host_ip(self->client);
            else  if ( ! g_inet_address_get_is_loopback(inet_address) )
                ip = ip_ = g_inet_address_to_string(inet_address);
            if ( ip != NULL )
                locations = g_list_prepend(locations, g_strdup_printf(EVP_SERVICE_NAME "://%s:%u", ip, g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(address))));
            g_free(ip_);
        }
        break;
        default:
        break;
        }

        g_object_unref(address);
    }

    return locations;
}

gboolean
_eventd_evp_ssdp_add_sockets(EventdEvpSSDPContext *self, const gchar *name, GList *sockets)
{
    NkUuid uuid = {{ 0 }};
    gchar *usn;
    if ( ! nk_uuid_parse(&uuid, EVP_SSDP_NS_UUID) )
        g_return_val_if_reached(FALSE);
    nk_uuid_from_name(&uuid, name, -1);
    usn = g_strdup_printf("uuid:%s::" EVP_SSDP_URN, uuid.string);

    gboolean ret = FALSE;

    GList *locations;
    locations = _eventd_evp_ssdp_sockets_to_locations(self, sockets);

    if ( locations != NULL )
    {
        gssdp_resource_group_add_resource(self->group, EVP_SSDP_URN, usn, locations);
        ret = TRUE;
    }

    g_free(usn);
    return ret;
}

EventdEvpSSDPContext *
eventd_evp_ssdp_start(const gchar *name, GList *sockets)
{
    EventdEvpSSDPContext *self;
    GError *error = NULL;

    if ( sockets == NULL )
        return NULL;

    self = g_new0(EventdEvpSSDPContext, 1);


    self->client = gssdp_client_new(NULL, NULL, &error);
    if ( self->client == NULL )
    {
        g_warning("Couldn't initialize GSSDP: %s", error->message);
        g_clear_error(&error);
        eventd_evp_ssdp_stop(self);
        return NULL;
    }

    self->group = gssdp_resource_group_new(self->client);
    if ( ! _eventd_evp_ssdp_add_sockets(self, name, sockets) )
    {
        g_warning("Couldn't add sockets to SSDP resource group");
        eventd_evp_ssdp_stop(self);
        return NULL;
    }
    gssdp_resource_group_set_available(self->group, TRUE);

    return self;
}

void
eventd_evp_ssdp_stop(EventdEvpSSDPContext *self)
{
    if ( self == NULL )
        return;

    if ( self->client != NULL )
    {
        g_object_unref(self->group);
        g_object_unref(self->client);
    }

    g_free(self);
}

#else /* ! ENABLE_SSDP */
EventdEvpSSDPContext *eventd_evp_ssdp_start(const gchar *name, GList *sockets) { return NULL; }
void eventd_evp_ssdp_stop(EventdEvpSSDPContext *self) {}
#endif /* ! ENABLE_SSDP */
