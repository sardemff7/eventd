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

#include <errno.h>
#include <string.h>

#include <glib.h>
#if ENABLE_GIO_UNIX
#include <gio/gunixsocketaddress.h>
#include <glib/gstdio.h>
#endif /* ENABLE_GIO_UNIX */

#if ENABLE_SYSTEMD
#include <sys/socket.h>
#include <systemd/sd-daemon.h>
#endif /* ENABLE_SYSTEMD */

#include "types.h"

#include "sockets.h"

struct _EventdSockets {
    GList *list;
};

GSocket *
eventd_sockets_get_inet_socket(EventdSockets *sockets, guint16 port)
{
    GSocket *socket = NULL;
    GError *error = NULL;
    GInetAddress *inet_address = NULL;
    GSocketAddress *address = NULL;
    GList *socket_;

    if ( port == 0 )
        goto fail;

    for ( socket_ = sockets->list ; socket_ != NULL ; socket_ = g_list_next(socket_) )
    {
        GSocketFamily family;

        family = g_socket_get_family(socket_->data);
        if ( ( family != G_SOCKET_FAMILY_IPV4 ) && ( family != G_SOCKET_FAMILY_IPV6 ) )
            continue;

        address = g_socket_get_local_address(socket_->data, &error);
        if ( address == NULL )
        {
            g_warning("Couldn’t get socket local address: %s", error->message);
            continue;
        }

        if ( g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(address)) == port )
        {
            socket = socket_->data;
            sockets->list = g_list_delete_link(sockets->list, socket_);
            return socket;
        }
    }

    if ( ( socket = g_socket_new(G_SOCKET_FAMILY_IPV6, G_SOCKET_TYPE_STREAM, 0, &error)  ) == NULL )
    {
        g_warning("Unable to create an IPv6 socket: %s", error->message);
        goto fail;
    }

    inet_address = g_inet_address_new_any(G_SOCKET_FAMILY_IPV6);
    address = g_inet_socket_address_new(inet_address, port);
    if ( ! g_socket_bind(socket, address, TRUE, &error) )
    {
        g_warning("Unable to bind the IPv6 socket: %s", error->message);
        goto fail;
    }

    if ( ! g_socket_listen(socket, &error) )
    {
        g_warning("Unable to listen with the IPv6 socket: %s", error->message);
        goto fail;
    }

    return socket;

fail:
    g_free(socket);
    g_clear_error(&error);
    return NULL;
}

GSocket *
eventd_sockets_get_unix_socket(EventdSockets *sockets, const gchar *path, gboolean take_over_socket, gboolean *created)
{
#if ENABLE_GIO_UNIX
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
            g_warning("Couldn’t get socket local address: %s", error->message);
            continue;
        }

        if ( g_strcmp0(path, g_unix_socket_address_get_path(G_UNIX_SOCKET_ADDRESS(address))) == 0 )
        {
            socket = socket_->data;
            sockets->list = g_list_remove_link(sockets->list, socket_);
            g_list_free_1(socket_);
            if ( created != NULL )
                *created = FALSE;
            return socket;
        }
    }

    if ( ( socket = g_socket_new(G_SOCKET_FAMILY_UNIX, G_SOCKET_TYPE_STREAM, 0, &error)  ) == NULL )
    {
        g_warning("Unable to create an UNIX socket: %s", error->message);
        goto fail;
    }

    if ( g_file_test(path, G_FILE_TEST_EXISTS) && ( ! g_file_test(path, G_FILE_TEST_IS_DIR|G_FILE_TEST_IS_REGULAR) ) )
    {
        if ( take_over_socket )
            g_unlink(path);
        else
            goto fail;
    }

    address = g_unix_socket_address_new(path);
    if ( ! g_socket_bind(socket, address, TRUE, &error) )
    {
        g_warning("Unable to bind the UNIX socket: %s", error->message);
        goto fail;
    }

    if ( ! g_socket_listen(socket, &error) )
    {
        g_warning("Unable to listen with the UNIX socket: %s", error->message);
        goto fail;
    }

    if ( created != NULL )
        *created = TRUE;
    return socket;

fail:
    if ( socket != NULL )
        g_object_unref(socket);
    g_clear_error(&error);
#endif /* ENABLE_GIO_UNIX */
    if ( created != NULL )
        *created = FALSE;
    return NULL;
}

EventdSockets *
eventd_sockets_new()
{
    EventdSockets *sockets;

#if ENABLE_SYSTEMD
    GError *error = NULL;
    GSocket *socket;
    gint n;
    gint r;
    gint fd;
#endif /* ENABLE_SYSTEMD */

    sockets = g_new0(EventdSockets, 1);

#if ENABLE_SYSTEMD
    n = sd_listen_fds(TRUE);
    if ( n < 0 )
        g_warning("Failed to acquire systemd sockets: %s", strerror(-n));

    for ( fd = SD_LISTEN_FDS_START ; fd < SD_LISTEN_FDS_START + n ; ++fd )
    {
        r = sd_is_socket(fd, AF_UNSPEC, SOCK_STREAM, 1);
        if ( r < 0 )
        {
            g_warning("Failed to verify systemd socket type: %s", strerror(-r));
            continue;
        }

        if ( r == 0 )
            continue;

        if ( ( socket = g_socket_new_from_fd(fd, &error) ) == NULL )
            g_warning("Failed to take a socket from systemd: %s", error->message);
        else
            sockets->list = g_list_prepend(sockets->list, socket);
    }
#endif /* ENABLE_SYSTEMD */

    return sockets;
}

void
eventd_sockets_free_all(EventdSockets *sockets)
{
    g_list_free_full(sockets->list, g_object_unref);

    g_free(sockets);
}
