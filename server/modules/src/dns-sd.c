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

#include <avahi-common/error.h>
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-client/lookup.h>
#include <avahi-glib/glib-watch.h>
#include <avahi-glib/glib-malloc.h>

#include <eventd-sd-module.h>

struct _EventdSdModuleContext {
    const EventdSdModuleControlInterface *control;
    gchar *name;
    GList *addresses;
    GHashTable *servers;
    AvahiGLibPoll *glib_poll;
    AvahiClient *client;
    AvahiEntryGroup *group;
    AvahiServiceBrowser *browser;
};

static void
_eventd_sd_dns_sd_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, void *user_data)
{
}

static void
_eventd_sd_dns_sd_create_group(EventdSdModuleContext *self, AvahiClient *client)
{
    GList *address_;
    int error;

    self->group = avahi_entry_group_new(client, _eventd_sd_dns_sd_group_callback, self);
    if ( self->group == NULL )
    {
        g_warning("Couldn't create avahi entry group: %s", avahi_strerror(avahi_client_errno(client)));
        return;
    }

    for ( address_ = self->addresses ; address_ != NULL ; address_ = g_list_next(address_) )
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

        if ( ( error = avahi_entry_group_add_service(self->group, AVAHI_IF_UNSPEC, proto, 0, self->name, EVP_SERVICE_TYPE, NULL, NULL, g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(address)), NULL) ) < 0 )
            g_warning("Couldn't add " EVP_SERVICE_TYPE " service: %s", avahi_strerror(error));

        g_object_unref(address);
    }
    g_list_free(self->addresses);
    self->addresses = NULL;

    if ( ! avahi_entry_group_is_empty(self->group) )
    {
        if ( ( error = avahi_entry_group_commit(self->group) ) == 0 )
            return;
        g_warning("Couldn't commit entry group: %s", avahi_strerror(error));
    }
    avahi_entry_group_free(self->group);
}

static void
_eventd_sd_dns_sd_service_resolve_callback(AvahiServiceResolver *r, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event, const gchar *name, const gchar *type, const gchar *domain, const gchar *host_name, const AvahiAddress *address, guint16 port, AvahiStringList *txt, AvahiLookupResultFlags flags, void *user_data)
{
    EventdSdModuleContext *self = user_data;
    EventdRelayServer *server;

    server = g_hash_table_lookup(self->servers, name);
    switch ( event )
    {
    case AVAHI_RESOLVER_FAILURE:
        g_warning("Service '%s', resolver failure: %s", name, avahi_strerror(avahi_client_errno(avahi_service_resolver_get_client(r))));
    break;
    case AVAHI_RESOLVER_FOUND:
#ifdef EVENTD_DEBUG
        g_debug("Service '%s' resolved: [%s]:%" G_GUINT16_FORMAT, name, host_name, port);
#endif /* EVENTD_DEBUG */
        if ( ! self->control->server_has_address(server) )
        {
            self->control->server_set_address(server, g_network_address_new(host_name, port));
            self->control->server_start(server);
        }
    default:
    break;
    }

    avahi_service_resolver_free(r);
}

static void
_eventd_sd_dns_sd_service_browser_callback(AvahiServiceBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const gchar *name, const gchar *type, const gchar *domain, AvahiLookupResultFlags flags, void *user_data)
{
    EventdSdModuleContext *self = user_data;
    EventdRelayServer *server;

    switch ( event )
    {
    case AVAHI_BROWSER_FAILURE:
        g_warning("Avahi Browser failure: %s", avahi_strerror(avahi_client_errno(self->client)));
        avahi_service_browser_free(self->browser);
        self->browser = NULL;
    break;
    case AVAHI_BROWSER_NEW:
#ifdef EVENTD_DEBUG
        g_debug("Service found in '%s' domain: %s", domain, name);
#endif /* EVENTD_DEBUG */
        if ( g_strcmp0(name, self->name) == 0 )
            break;
        if ( ( server = g_hash_table_lookup(self->servers, name) ) != NULL )
            avahi_service_resolver_new(self->client, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, 0, _eventd_sd_dns_sd_service_resolve_callback, self);
    break;
    case AVAHI_BROWSER_REMOVE:
#ifdef EVENTD_DEBUG
        g_debug("Service removed in '%s' domain: %s", domain, name);
#endif /* EVENTD_DEBUG */
        if ( g_strcmp0(name, self->name) == 0 )
            break;
        if ( ( ( server = g_hash_table_lookup(self->servers, name) ) != NULL ) && ( self->control->server_has_address(server) ) )
        {
            self->control->server_stop(server);
            self->control->server_set_address(server, NULL);
        }
    break;
    case AVAHI_BROWSER_ALL_FOR_NOW:
    case AVAHI_BROWSER_CACHE_EXHAUSTED:
    break;
    }
}

static void
_eventd_sd_dns_sd_client_callback(AvahiClient *client, AvahiClientState state, void *user_data)
{
    EventdSdModuleContext *self = user_data;

    switch ( state )
    {
    case AVAHI_CLIENT_S_RUNNING:
        if ( ( self->name != NULL ) && ( self->addresses != NULL ) )
            _eventd_sd_dns_sd_create_group(self, client);
        self->browser = avahi_service_browser_new(client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, EVP_SERVICE_TYPE, NULL, 0, _eventd_sd_dns_sd_service_browser_callback, self);
    case AVAHI_CLIENT_S_REGISTERING:
    break;
    case AVAHI_CLIENT_FAILURE:
        avahi_client_free(client);
        self->client = NULL;
    break;
    default:
    break;
    }
}

static GList *
_eventd_sd_dns_sd_sockets_to_addresses(GList *sockets)
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

static EventdSdModuleContext *
_eventd_sd_dns_sd_init(const EventdSdModuleControlInterface *control, GList *sockets)
{
    EventdSdModuleContext *self;

    avahi_set_allocator(avahi_glib_allocator());

    self = g_new0(EventdSdModuleContext, 1);
    self->control = control;
    self->glib_poll = avahi_glib_poll_new(NULL, G_PRIORITY_DEFAULT);

    self->servers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    self->addresses = _eventd_sd_dns_sd_sockets_to_addresses(sockets);;

    return self;
}

static void
_eventd_sd_dns_sd_uninit(EventdSdModuleContext *self)
{
    g_list_free_full(self->addresses, g_object_unref);
    g_hash_table_unref(self->servers);

    avahi_glib_poll_free(self->glib_poll);

    g_free(self->name);

    g_free(self);
}

static void
_eventd_sd_dns_sd_set_publish_name(EventdSdModuleContext *self, const gchar *publish_name)
{
    g_free(self->name);
    self->name = g_strdup(publish_name);
}

void
_eventd_sd_dns_sd_monitor_server(EventdSdModuleContext *self, const gchar *name, EventdRelayServer *server)
{
    g_hash_table_insert(self->servers, g_strdup(name), server);
}


static void _eventd_sd_dns_sd_stop(EventdSdModuleContext *self);
static void
_eventd_sd_dns_sd_start(EventdSdModuleContext *self)
{
    int error;

    self->client = avahi_client_new(avahi_glib_poll_get(self->glib_poll), 0, _eventd_sd_dns_sd_client_callback, self, &error);

    if ( self->client == NULL )
    {
        g_warning("Couldn't initialize Avahi: %s", avahi_strerror(error));
        _eventd_sd_dns_sd_stop(self);
    }
}

static void
_eventd_sd_dns_sd_stop(EventdSdModuleContext *self)
{
    if ( self->client != NULL )
        avahi_client_free(self->client);
    self->client = NULL;

    g_hash_table_remove_all(self->servers);
}

EVENTD_EXPORT
void
eventd_sd_module_get_info(EventdSdModule *module)
{
    module->init = _eventd_sd_dns_sd_init;
    module->uninit = _eventd_sd_dns_sd_uninit;

    module->set_publish_name = _eventd_sd_dns_sd_set_publish_name;
    module->monitor_server = _eventd_sd_dns_sd_monitor_server;

    module->start = _eventd_sd_dns_sd_start;
    module->stop = _eventd_sd_dns_sd_stop;
}
