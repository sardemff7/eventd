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

GList *
eventd_sockets_get_systemd(gchar **private_socket, gchar **unix_socket)
{
    GError *error = NULL;
    GList *sockets = NULL;
    GSocket *socket = NULL;
    gint r, n;
    gint fd;

    g_free(*unix_socket);
    *unix_socket = NULL;
    g_free(*private_socket);
    *private_socket = NULL;

    n = sd_listen_fds(TRUE);
    if ( n < 0 )
    {
        g_warning("Failed to acquire systemd socket: %s", strerror(-n));
        return NULL;
    }

    if ( n <= 0 )
    {
        g_warning("No socket received.");
        return NULL;
    }

    for ( fd = SD_LISTEN_FDS_START ; fd < SD_LISTEN_FDS_START + n ; ++fd )
    {
        r = sd_is_socket(fd, AF_UNSPEC, SOCK_STREAM, 1);
        if ( r < 0 )
        {
            g_warning("Failed to verify systemd socket type: %s", strerror(-r));
            return NULL;
        }


        if ( r <= 0 )
        {
            g_warning("Passed socket has wrong type.");
            return NULL;
        }
    }

    for ( fd = SD_LISTEN_FDS_START ; fd < SD_LISTEN_FDS_START + n ; ++fd )
    {
        if ( ( socket = g_socket_new_from_fd(fd, &error) ) == NULL )
        {
            g_warning("Failed to take a socket from systemd: %s", error->message);
            continue;
        }
        sockets = g_list_prepend(sockets, socket);
    }

    return sockets;
}
#endif /* ENABLE_SYSTEMD */

static GSocket *
_eventd_socketsget_inet_socket(guint16 port)
{
    GSocket *socket = NULL;
    GError *error = NULL;
    GInetAddress *inet_address = NULL;
    GSocketAddress *address = NULL;

    if ( port == 0 )
        goto fail;

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

static GSocket *
_eventd_sockets_get_unix_socket(gchar *path, gboolean take_over_socket)
{
#if ENABLE_GIO_UNIX
    GSocket *socket = NULL;
    GError *error = NULL;
    GSocketAddress *address = NULL;

    if ( path == NULL )
        goto fail;

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

    return socket;

fail:
    if ( socket != NULL )
        g_object_unref(socket);
    g_clear_error(&error);
#endif /* ENABLE_GIO_UNIX */
    return NULL;
}

GList *
eventd_sockets_get_all(
    guint16 bind_port,

    gboolean no_network,
    gboolean no_unix,
    gchar **private_socket,
    gchar **unix_socket,
    gboolean take_over_socket)
{
    GList *sockets = NULL;
    GSocket *socket = NULL;
#if ENABLE_GIO_UNIX
    gchar *run_dir = NULL;

    run_dir = g_build_filename(g_get_user_runtime_dir(), PACKAGE_NAME, NULL);
    if ( g_mkdir_with_parents(run_dir, 0755) < 0 )
    {
        g_warning("Couldn’t create the run dir '%s': %s", run_dir, strerror(errno));
        goto no_run_dir;
    }

    if ( *private_socket == NULL )
        *private_socket = g_build_filename(run_dir, "private", NULL);
    if ( ( socket = _eventd_sockets_get_unix_socket(*private_socket, take_over_socket) ) != NULL )
        sockets = g_list_prepend(sockets, socket);
    else
        g_warning("Couldn’t create private socket");

    if ( no_unix )
    {
        g_free(*unix_socket);
        *unix_socket = NULL;
    }
    else
    {
        if ( *unix_socket == NULL )
            *unix_socket = g_build_filename(run_dir, UNIX_SOCKET, NULL);

        if ( ( socket = _eventd_sockets_get_unix_socket(*unix_socket, take_over_socket) ) != NULL )
            sockets = g_list_prepend(sockets, socket);
        else
        {
            g_free(*unix_socket);
            *unix_socket = NULL;
        }
    }

no_run_dir:
    g_free(run_dir);

    if ( no_network )
        bind_port = 0;
#endif /* ENABLE_GIO_UNIX */

    if ( ( bind_port > 0 ) && ( ( socket = _eventd_socketsget_inet_socket(bind_port) ) != NULL ) )
        sockets = g_list_prepend(sockets, socket);

    return sockets;
}

void
eventd_sockets_free_all(GList *sockets, gchar *private_socket, gchar *unix_socket)
{
    g_list_free_full(sockets, g_object_unref);

    if ( unix_socket != NULL )
    {
        g_unlink(unix_socket);
        g_free(unix_socket);
    }

    if ( private_socket != NULL )
    {
        g_unlink(private_socket);
        g_free(private_socket);
    }
}
