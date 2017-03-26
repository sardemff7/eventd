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
#include <nkutils-uuid.h>

#include <libgssdp/gssdp.h>

#include "eventd-sd-module.h"

struct _EventdSdModuleContext {
    const EventdSdModuleControlInterface *control;
    NkUuid ns_uuid;
    GSSDPClient *client;
    gchar *usn;
    GList *locations;
    GSSDPResourceGroup *group;
    GSSDPResourceBrowser *browser;
    GHashTable *servers;
};

static void
_eventd_sd_ssdp_resource_available(EventdSdModuleContext *self, gchar *usn, GList *locations, GSSDPResourceBrowser *b)
{
    EventdRelayServer *server;
    if ( g_strcmp0(usn, self->usn) == 0 )
        return;
    if ( ( server = g_hash_table_lookup(self->servers, usn) ) == NULL )
        return;

#ifdef EVENTD_DEBUG
        g_debug("Found resource '%s'", usn);
#endif /* EVENTD_DEBUG */

    if ( self->control->server_has_address(server) )
        return;

    GList *location_;
    GSocketConnectable *address = NULL;
    for ( location_ = locations ; ( address == NULL ) && ( location_ != NULL ) ; location_ = g_list_next(location_) )
    {
        const gchar *location = location_->data;
        if ( g_str_has_prefix(location, "evp://") )
        {
            address = g_network_address_parse(location + strlen("evp://"), 0, NULL);
            if ( ( address != NULL ) && ( g_network_address_get_port(G_NETWORK_ADDRESS(address)) == 0 ) )
                address = (g_object_unref(address), NULL);
        }
    }
    if ( address == NULL )
    {
        g_warning("Resource '%s': no valid location", usn);
        return;
    }

    self->control->server_set_address(server, address);
    self->control->server_start(server, TRUE);
}

static void
_eventd_sd_ssdp_resource_unavailable(EventdSdModuleContext *self, gchar *usn, GSSDPResourceBrowser *b)
{
    EventdRelayServer *server;
    if ( g_strcmp0(usn, self->usn) == 0 )
        return;
    if ( ( server = g_hash_table_lookup(self->servers, usn) ) == NULL )
        return;

#ifdef EVENTD_DEBUG
        g_debug("Resource '%s' is gone", usn);
#endif /* EVENTD_DEBUG */

    if ( ! self->control->server_has_address(server) )
        return;

    self->control->server_stop(server);
    self->control->server_set_address(server, NULL);
}

static GList *
_eventd_sd_ssdp_get_locations(GSSDPClient *client, GList *sockets)
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
                ip = gssdp_client_get_host_ip(client);
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

static EventdSdModuleContext *
_eventd_sd_ssdp_init(const EventdSdModuleControlInterface *control, GList *sockets)
{
    EventdSdModuleContext *self;

    NkUuid uuid;
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

    self = g_new0(EventdSdModuleContext, 1);
    self->control = control;
    self->ns_uuid = uuid;
    self->client = client;

    self->browser = gssdp_resource_browser_new(self->client, EVP_SSDP_URN);
    g_signal_connect_swapped(self->browser, "resource-available", G_CALLBACK(_eventd_sd_ssdp_resource_available), self);
    g_signal_connect_swapped(self->browser, "resource-unavailable", G_CALLBACK(_eventd_sd_ssdp_resource_unavailable), self);

    self->servers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    self->locations = _eventd_sd_ssdp_get_locations(client, sockets);

    return self;
}

static void
_eventd_sd_ssdp_uninit(EventdSdModuleContext *self)
{
    g_list_free_full(self->locations, g_free);
    g_hash_table_unref(self->servers);

    g_object_unref(self->browser);

    g_object_unref(self->client);

    g_free(self);
}

static void
_eventd_sd_ssdp_set_publish_name(EventdSdModuleContext *self, const gchar *publish_name)
{
    NkUuid uuid = self->ns_uuid;
    nk_uuid_from_name(&uuid, publish_name, -1);
    self->usn = g_strdup_printf("uuid:%s::" EVP_SSDP_URN, uuid.string);
}

static void
_eventd_sd_ssdp_monitor_server(EventdSdModuleContext *self, const gchar *name, EventdRelayServer *server)
{
    NkUuid uuid = self->ns_uuid;
    gchar *usn;
    nk_uuid_from_name(&uuid, name, -1);

    usn = g_strdup_printf("uuid:%s::" EVP_SSDP_URN, uuid.string);

    g_hash_table_insert(self->servers, usn, server);
}

static void
_eventd_sd_ssdp_start(EventdSdModuleContext *self)
{
    gssdp_resource_browser_set_active(self->browser, TRUE);

    if ( ( self->usn == NULL ) || ( self->locations == NULL ) )
        return;

    self->group = gssdp_resource_group_new(self->client);
    gssdp_resource_group_add_resource(self->group, EVP_SSDP_URN, self->usn, self->locations);
    gssdp_resource_group_set_available(self->group, TRUE);
}

static void
_eventd_sd_ssdp_stop(EventdSdModuleContext *self)
{
    gssdp_resource_browser_set_active(self->browser, FALSE);

    if ( self->group != NULL )
    {
        g_object_unref(self->group);
        self->group = NULL;
    }
}

EVENTD_EXPORT
void
eventd_sd_module_get_info(EventdSdModule *module)
{
    module->init = _eventd_sd_ssdp_init;
    module->uninit = _eventd_sd_ssdp_uninit;

    module->set_publish_name = _eventd_sd_ssdp_set_publish_name;
    module->monitor_server = _eventd_sd_ssdp_monitor_server;

    module->start = _eventd_sd_ssdp_start;
    module->stop = _eventd_sd_ssdp_stop;
}
