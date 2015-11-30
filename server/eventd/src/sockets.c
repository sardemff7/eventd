/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2015 Quentin "Sardem FF7" Glidic
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
#include <glib/gstdio.h>
#include <gio/gio.h>
#ifdef G_OS_UNIX
#include <gio/gunixsocketaddress.h>
#endif /* G_OS_UNIX */

#ifdef ENABLE_SYSTEMD
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif /* HAVE_SYS_SOCKET_H */
#include <systemd/sd-daemon.h>
#endif /* ENABLE_SYSTEMD */

#include "types.h"

#include "sockets.h"

struct _EventdSockets {
    gint systemd_fds;
    GList *list;
    GSList *created;
};

GList *
eventd_sockets_get_systemd_sockets(EventdSockets *sockets)
{
    GList *list = sockets->list;
    sockets->list = NULL;
    return list;
}

static gboolean
_eventd_sockets_inet_address_equal(GInetSocketAddress *socket_address1, GInetAddress *address2, guint16 port)
{
    gboolean ret = FALSE;
    if ( g_inet_socket_address_get_port(socket_address1) == port )
        ret = g_inet_address_equal(g_inet_socket_address_get_address(socket_address1), address2);
    g_object_unref(socket_address1);
    return ret;
}

static GSocket *
_eventd_sockets_get_inet_socket(EventdSockets *sockets, GInetAddress *inet_address, guint16 port)
{
    GSocket *socket = NULL;
    GError *error = NULL;
    GSocketAddress *address = NULL;
    GList *socket_;

    if ( port != 0 )
    for ( socket_ = sockets->list ; socket_ != NULL ; socket_ = g_list_next(socket_) )
    {
        GSocketFamily family;

        family = g_socket_get_family(socket_->data);
        if ( ( family != G_SOCKET_FAMILY_IPV4 ) && ( family != G_SOCKET_FAMILY_IPV6 ) )
            continue;

        address = g_socket_get_local_address(socket_->data, &error);
        if ( address == NULL )
        {
            g_warning("Couldn't get socket local address: %s", error->message);
            continue;
        }

        if ( _eventd_sockets_inet_address_equal(G_INET_SOCKET_ADDRESS(address), inet_address, port) )
        {
            socket = socket_->data;
            sockets->list = g_list_delete_link(sockets->list, socket_);
            return socket;
        }
    }
    address = NULL;

    if ( ( socket = g_socket_new(g_inet_address_get_family(inet_address), G_SOCKET_TYPE_STREAM, 0, &error)  ) == NULL )
    {
        g_warning("Unable to create an IP socket: %s", error->message);
        goto fail;
    }

    address = g_inet_socket_address_new(inet_address, port);
    if ( ! g_socket_bind(socket, address, TRUE, &error) )
    {
        g_warning("Unable to bind the IP socket: %s", error->message);
        goto fail;
    }

    if ( ! g_socket_listen(socket, &error) )
    {
        g_warning("Unable to listen with the IP socket: %s", error->message);
        goto fail;
    }

    g_object_unref(address);
    g_object_unref(inet_address);
    return socket;

fail:
    if ( socket != NULL )
        g_object_unref(socket);
    if ( address != NULL )
        g_object_unref(address);
    g_clear_error(&error);
    g_object_unref(inet_address);
    return NULL;
}

GList *
eventd_sockets_get_inet_sockets(EventdSockets *sockets, const gchar *address, guint16 port)
{
    GSocket *socket;

    if ( address == NULL )
        socket = _eventd_sockets_get_inet_socket(sockets, g_inet_address_new_any(G_SOCKET_FAMILY_IPV6), port);
    else if ( g_strcmp0(address, "localhost") == 0 )
    {
        socket = _eventd_sockets_get_inet_socket(sockets, g_inet_address_new_loopback(G_SOCKET_FAMILY_IPV6), port);
    }
    else
    {
        GList *ret_sockets = NULL;
        GResolver *resolver;
        GList *inet_addresses;
        GError *error = NULL;

        resolver = g_resolver_get_default();
        inet_addresses = g_resolver_lookup_by_name(resolver, address, NULL, &error);
        if ( inet_addresses == NULL )
        {
            g_warning("Couldn't resolve '%s' to an address: %s", address, error->message);
            g_clear_error(&error);
            return NULL;
        }

        GList *inet_address_;

        for ( inet_address_ = inet_addresses ; inet_address_ != NULL ; inet_address_ = g_list_next(inet_address_) )
        {
            socket = _eventd_sockets_get_inet_socket(sockets, g_object_ref(inet_address_->data), port);
            if ( socket != NULL )
                ret_sockets = g_list_prepend(ret_sockets, socket);
        }

        g_resolver_free_addresses(inet_addresses);

        return ret_sockets;
    }
    if ( socket == NULL )
        return NULL;
    return g_list_prepend(NULL, socket);
}

GSocket *
eventd_sockets_get_inet_socket_file(EventdSockets *sockets, const gchar *file, gboolean take_over_socket)
{
    if ( g_file_test(file, G_FILE_TEST_EXISTS) && ( ! take_over_socket ) )
    {
        g_warning("File to write port exists already");
        return NULL;
    }

    GSocket *socket;
    GSocketAddress *address;
    gchar port_str[6];
    GError *error = NULL;

    socket = _eventd_sockets_get_inet_socket(sockets, g_inet_address_new_loopback(G_SOCKET_FAMILY_IPV6), 0);
    if ( socket == NULL )
        return NULL;

    address = g_socket_get_local_address(socket, &error);
    if ( address == NULL )
    {
        g_warning("Couldn't get socket local address: %s", error->message);
        goto fail;
    }
    g_sprintf(port_str, "%u", g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(address)));
    g_object_unref(address);

    if ( g_file_set_contents(file, port_str, -1, &error) )
    {
        sockets->created = g_slist_prepend(sockets->created, g_strdup(file));
        return socket;
}

    g_warning("Couldn't write port to file: %s", error->message);

fail:
    g_clear_error(&error);
    g_object_unref(socket);
    return NULL;
}

#ifdef G_OS_UNIX
GSocket *
eventd_sockets_get_unix_socket(EventdSockets *sockets, const gchar *path, gboolean take_over_socket)
{
    GSocket *socket = NULL;
    GError *error = NULL;
    GSocketAddress *address = NULL;
    GList *socket_;

    if ( path == NULL )
        goto fail;

    for ( socket_ = sockets->list ; socket_ != NULL ; socket_ = g_list_next(socket_) )
    {
        if ( g_socket_get_family(socket_->data) != G_SOCKET_FAMILY_UNIX )
            continue;

        address = g_socket_get_local_address(socket_->data, &error);
        if ( address == NULL )
        {
            g_warning("Couldn't get socket local address: %s", error->message);
            g_clear_error(&error);
            continue;
        }

        if ( g_strcmp0(path, g_unix_socket_address_get_path(G_UNIX_SOCKET_ADDRESS(address))) == 0 )
        {
            socket = socket_->data;
            sockets->list = g_list_delete_link(sockets->list, socket_);
            return socket;
        }
    }

    if ( g_file_test(path, G_FILE_TEST_EXISTS) && ( ! g_file_test(path, G_FILE_TEST_IS_DIR|G_FILE_TEST_IS_REGULAR) ) )
    {
        if ( take_over_socket )
            g_unlink(path);
        else
        {
            g_warning("Socket %s already exists", path);
            return NULL;
        }
    }

    if ( ( socket = g_socket_new(G_SOCKET_FAMILY_UNIX, G_SOCKET_TYPE_STREAM, 0, &error)  ) == NULL )
    {
        g_warning("Unable to create an UNIX socket: %s", error->message);
        goto fail;
    }

    if ( g_str_has_prefix(path, "@") )
        address = g_unix_socket_address_new_with_type(path + 1, -1, G_UNIX_SOCKET_ADDRESS_ABSTRACT);
    else
        address = g_unix_socket_address_new(path);
    if ( ! g_socket_bind(socket, address, TRUE, &error) )
    {
        g_warning("Unable to bind the UNIX socket: %s", error->message);
        goto fail;
    }
    g_object_unref(address);

    if ( ! g_socket_listen(socket, &error) )
    {
        g_warning("Unable to listen with the UNIX socket: %s", error->message);
        goto fail;
    }

    sockets->created = g_slist_prepend(sockets->created, g_strdup(path));
    return socket;

fail:
    if ( address != NULL )
        g_object_unref(address);
    if ( socket != NULL )
        g_object_unref(socket);
    g_clear_error(&error);
    return NULL;
}
#endif /* G_OS_UNIX */

EventdSockets *
eventd_sockets_new(void)
{
    EventdSockets *sockets;

    sockets = g_new0(EventdSockets, 1);

#ifdef ENABLE_SYSTEMD
    if ( sockets->systemd_fds < 1 )
    {
        sockets->systemd_fds = sd_listen_fds(TRUE);
        if ( sockets->systemd_fds < 0 )
            g_warning("Failed to acquire systemd sockets: %s", g_strerror(-sockets->systemd_fds));
    }

    GError *error = NULL;
    gint fd;
    for ( fd = SD_LISTEN_FDS_START ; fd < SD_LISTEN_FDS_START + sockets->systemd_fds ; ++fd )
    {
        gint r;
        r = sd_is_socket(fd, AF_UNSPEC, SOCK_STREAM, 1);
        if ( r < 0 )
        {
            g_warning("Failed to verify systemd socket type: %s", g_strerror(-r));
            continue;
        }

        if ( r == 0 )
            continue;

        GSocket *socket;

        if ( ( socket = g_socket_new_from_fd(fd, &error) ) == NULL )
        {
            g_warning("Failed to take a socket from systemd: %s", error->message);
            g_clear_error(&error);
        }
        else
            sockets->list = g_list_prepend(sockets->list, socket);
    }
#endif /* ENABLE_SYSTEMD */

    return sockets;
}

static void
_eventd_sockets_created_socket_free(gpointer data)
{
    gchar *path = data;
    g_unlink(path);
    g_free(path);
}

void
eventd_sockets_free(EventdSockets *sockets)
{
    g_list_free_full(sockets->list, g_object_unref);

    g_slist_free_full(sockets->created, _eventd_sockets_created_socket_free);

    g_free(sockets);
}
