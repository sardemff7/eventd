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

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <nkutils-uuid.h>

#include <libeventd-event.h>
#include <eventd-plugin.h>

#include "server.h"

#include "ssdp.h"

#ifdef ENABLE_SSDP
#include <libgssdp/gssdp.h>

struct _EventdRelaySSDP {
    NkUuid ns_uuid;
    GSSDPClient *client;
    GSSDPResourceBrowser *browser;
    GHashTable *servers;
};

static void
_eventd_relay_ssdp_resource_available(EventdRelaySSDP *self, gchar *usn, GList *locations, GSSDPResourceBrowser *b)
{
    EventdRelayServer *server;
    if ( ( server = g_hash_table_lookup(self->servers, usn) ) == NULL )
        return;

#ifdef EVENTD_DEBUG
        g_debug("Found resource '%s'", usn);
#endif /* EVENTD_DEBUG */

    if ( eventd_relay_server_has_address(server) )
        return;

    GList *location;
    GSocketConnectable *address = NULL;
    for ( location = locations ; ( address == NULL ) && ( location != NULL ) ; location = g_list_next(location) )
    {
        if ( g_str_has_prefix(location->data, "evp://") )
        {
            address = g_network_address_parse(location->data + strlen("evp://"), 0, NULL);
            if ( ( address != NULL ) && ( g_network_address_get_port(G_NETWORK_ADDRESS(address)) == 0 ) )
                address = (g_object_unref(address), NULL);
        }
    }
    if ( address == NULL )
    {
        g_warning("Resource '%s': no valid location", usn);
        return;
    }

    eventd_relay_server_set_address(server, address);
    eventd_relay_server_start(server);
}

static void
_eventd_relay_ssdp_resource_unavailable(EventdRelaySSDP *self, gchar *usn, GSSDPResourceBrowser *b)
{
    EventdRelayServer *server;
    if ( ( server = g_hash_table_lookup(self->servers, usn) ) == NULL )
        return;

#ifdef EVENTD_DEBUG
        g_debug("Resource '%s' is gone", usn);
#endif /* EVENTD_DEBUG */

    if ( ! eventd_relay_server_has_address(server) )
        return;

    eventd_relay_server_stop(server);
    eventd_relay_server_set_address(server, NULL);
}

EventdRelaySSDP *
eventd_relay_ssdp_init(void)
{
    EventdRelaySSDP *self;

    NkUuid uuid = {{ 0 }};
    if ( ! nk_uuid_parse(&uuid, EVP_SSDP_NS_UUID) )
        g_return_val_if_reached(NULL);

    GSSDPClient *client;
    GError *error = NULL;

    client = gssdp_client_new(NULL, NULL, &error);
    if ( client == NULL )
    {
        g_warning("Couldn't initialize GSSDP: %s", error->message);
        g_clear_error(&error);
        return NULL;
    }

    self = g_new0(EventdRelaySSDP, 1);
    self->ns_uuid = uuid;
    self->client = client;

    self->browser = gssdp_resource_browser_new(self->client, EVP_SSDP_URN);
    g_signal_connect_swapped(self->browser, "resource-available", G_CALLBACK(_eventd_relay_ssdp_resource_available), self);
    g_signal_connect_swapped(self->browser, "resource-unavailable", G_CALLBACK(_eventd_relay_ssdp_resource_unavailable), self);

    self->servers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    return self;
}

void
eventd_relay_ssdp_uninit(EventdRelaySSDP *self)
{
    if ( self == NULL )
        return;

    g_hash_table_unref(self->servers);

    g_object_unref(self->client);

    g_free(self);
}

void
eventd_relay_ssdp_start(EventdRelaySSDP *self)
{
    if ( self == NULL )
        return;

    gssdp_resource_browser_set_active(self->browser, TRUE);
}

void
eventd_relay_ssdp_stop(EventdRelaySSDP *self)
{
    if ( self == NULL )
        return;

    gssdp_resource_browser_set_active(self->browser, FALSE);
}


void
eventd_relay_ssdp_monitor_server(EventdRelaySSDP *self, const gchar *name, EventdRelayServer *server)
{
    if ( self == NULL )
        return;

    NkUuid uuid = self->ns_uuid;
    gchar *usn;
    nk_uuid_from_name(&uuid, name, -1);

    usn = g_strdup_printf("uuid:%s::" EVP_SSDP_URN, uuid.string);

    g_hash_table_insert(self->servers, usn, server);
}

#else /* ! ENABLE_SSDP */
EventdRelaySSDP *eventd_relay_ssdp_init(void) { return NULL; }
void eventd_relay_ssdp_uninit(EventdRelaySSDP *self) {}

void eventd_relay_ssdp_start(EventdRelaySSDP *self) {}
void eventd_relay_ssdp_stop(EventdRelaySSDP *self) {}

void eventd_relay_ssdp_monitor_server(EventdRelaySSDP *self, const gchar *name, EventdRelayServer *server) {}
#endif /* ! ENABLE_SSDP */
