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
    GList *list;
    GSList *created;
};

static gboolean
_eventd_sockets_inet_address_equal(GInetSocketAddress *socket_address1, GInetAddress *address2, guint16 port)
{
    gboolean ret = FALSE;
    if ( g_inet_socket_address_get_port(socket_address1) == port )
        ret = g_inet_address_equal(g_inet_socket_address_get_address(socket_address1), address2);
    g_object_unref(socket_address1);
    return ret;
}

static gboolean
_eventd_sockets_add_inet_socket(EventdSockets *sockets, GList **list, GInetAddress *inet_address, guint16 port, gboolean ipv6_bound)
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
            sockets->list = g_list_remove_link(sockets->list, socket_);
            *list = g_list_concat(*list, socket_);
            return TRUE;
        }
    }
    address = NULL;

    gboolean ret = FALSE;

    if ( ( socket = g_socket_new(g_inet_address_get_family(inet_address), G_SOCKET_TYPE_STREAM, 0, &error)  ) == NULL )
    {
        g_warning("Unable to create an IP socket: %s", error->message);
        goto fail;
    }

    address = g_inet_socket_address_new(inet_address, port);
    if ( ! g_socket_bind(socket, address, TRUE, &error) )
    {
        if ( ( error->code == G_IO_ERROR_ADDRESS_IN_USE ) && ipv6_bound )
            ret = TRUE;
        else
            g_warning("Unable to bind the IP socket: %s", error->message);
        goto fail;
    }

    if ( ! g_socket_listen(socket, &error) )
    {
        g_warning("Unable to listen with the IP socket: %s", error->message);
        goto fail;
    }

    *list = g_list_prepend(*list, socket);
    socket = NULL;
    ret = TRUE;

fail:
    if ( socket != NULL )
        g_object_unref(socket);
    if ( address != NULL )
        g_object_unref(address);
    g_clear_error(&error);
    g_object_unref(inet_address);
    return ret;
}

static GList *
_eventd_sockets_get_inet_sockets(EventdSockets *sockets, const gchar *address, guint16 port)
{
    gboolean ret = FALSE;
    GList *list = NULL;

    if ( address == NULL )
    {
        if ( _eventd_sockets_add_inet_socket(sockets, &list, g_inet_address_new_any(G_SOCKET_FAMILY_IPV6), port, FALSE) )
            ret = _eventd_sockets_add_inet_socket(sockets, &list, g_inet_address_new_any(G_SOCKET_FAMILY_IPV4), port, TRUE);
    }
    else if ( g_strcmp0(address, "localhost") == 0 )
    {
        if ( _eventd_sockets_add_inet_socket(sockets, &list, g_inet_address_new_loopback(G_SOCKET_FAMILY_IPV6), port, FALSE) )
            ret = _eventd_sockets_add_inet_socket(sockets, &list, g_inet_address_new_loopback(G_SOCKET_FAMILY_IPV4), port, TRUE);
    }
    else
    {
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

        ret = TRUE;
        for ( inet_address_ = inet_addresses ; ret && ( inet_address_ != NULL ) ; inet_address_ = g_list_next(inet_address_) )
        {
            if ( ! _eventd_sockets_add_inet_socket(sockets, &list, g_object_ref(inet_address_->data), port, FALSE) )
                ret = FALSE;
        }

        g_resolver_free_addresses(inet_addresses);
    }
    if ( ret )
        return list;

    g_list_free_full(list, g_object_unref);
    return NULL;
}

static GList *
_eventd_sockets_get_inet_socket_file(EventdSockets *sockets, const gchar *file, gboolean take_over_socket)
{
    if ( g_file_test(file, G_FILE_TEST_EXISTS) && ( ! take_over_socket ) )
    {
        g_warning("File to write port exists already");
        return NULL;
    }

    GList *list = NULL;
    GSocket *socket;
    GSocketAddress *address;
    guint16 port;
    gchar port_str[6];
    GError *error = NULL;

    if ( !_eventd_sockets_add_inet_socket(sockets, &list, g_inet_address_new_loopback(G_SOCKET_FAMILY_IPV6), 0, FALSE) )
        return NULL;
    socket = list->data;

    address = g_socket_get_local_address(socket, &error);
    if ( address == NULL )
    {
        g_warning("Couldn't get socket local address: %s", error->message);
        goto fail;
    }
    port = g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(address));
    g_object_unref(address);

    g_sprintf(port_str, "%u", port);

    if ( g_file_set_contents(file, port_str, -1, &error) )
        sockets->created = g_slist_prepend(sockets->created, g_strdup(file));
    else
    {
        g_warning("Couldn't write port to file: %s", error->message);
        goto fail;
    }

    if ( ! _eventd_sockets_add_inet_socket(sockets, &list, g_inet_address_new_loopback(G_SOCKET_FAMILY_IPV4), port, TRUE) )
        goto fail;

    return list;
fail:
    g_clear_error(&error);
    g_list_free_full(list, g_object_unref);
    return NULL;
}

#ifdef G_OS_UNIX
static GList *
_eventd_sockets_get_unix_sockets(EventdSockets *sockets, const gchar *path, gboolean take_over_socket)
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
            sockets->list = g_list_remove_link(sockets->list, socket_);
            return socket_;
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
    return g_list_prepend(NULL, socket);

fail:
    if ( address != NULL )
        g_object_unref(address);
    if ( socket != NULL )
        g_object_unref(socket);
    g_clear_error(&error);
    return NULL;
}
#endif /* G_OS_UNIX */

static gboolean
_eventd_sockets_get_inet_address(const gchar *bind, gchar **address, guint16 *port)
{
    const gchar *address_port;

    address_port = g_strrstr(bind, ":");
    if ( address_port != NULL )
        ++address_port;
    else
        address_port = bind;

    gint64 parsed_value;

    parsed_value = g_ascii_strtoll(address_port, NULL, 10);
    *port = CLAMP(parsed_value, 0, 65535);

    if ( bind[0] == '[' )
    {
        /*
         * This is an IPv6 address
         * we remove the enclosing square bracets
         */
        ++bind;
        --address_port;
    }
    if ( --address_port > bind )
        *address = g_strndup(bind, address_port - bind);
    else
        *address = NULL;

    return TRUE;
}

GList *
eventd_sockets_get_binds(EventdSockets *self, const gchar * const *binds, const gchar *runtime_dir, gboolean take_over_socket)
{
    GList *sockets = NULL;
    const gchar * const * bind_;

    if ( binds == NULL )
        return NULL;

    for ( bind_ = binds ; *bind_ != NULL ; ++bind_ )
    {
        const gchar *bind = *bind_;

        if ( *bind == 0 )
            continue;

        GList *new_sockets = NULL;

        if ( g_strcmp0(bind, "all") == 0 )
        {
            new_sockets = self->list;
            self->list = NULL;
        }
        else if ( g_str_has_prefix(bind, "tcp:") )
        {
            if ( bind[4] == 0 )
                continue;
            gchar *address;
            guint16 port;

            if ( ! _eventd_sockets_get_inet_address(bind+4, &address, &port) )
                continue;

            new_sockets = _eventd_sockets_get_inet_sockets(self, address, port);
            g_free(address);
        }
        else if ( g_str_has_prefix(bind, "tcp-file:") )
        {
            if ( bind[9] == 0 )
                continue;

            new_sockets = _eventd_sockets_get_inet_socket_file(self, bind+9, take_over_socket);
        }
        else if ( g_str_has_prefix(bind, "tcp-file-runtime:") )
        {
            if ( bind[17] == 0 )
                continue;

            gchar *path;

            path = g_build_filename(runtime_dir, bind+17, NULL);
            new_sockets = _eventd_sockets_get_inet_socket_file(self, path, take_over_socket);
            g_free(path);
        }
#ifdef G_OS_UNIX
        else if ( g_str_has_prefix(bind, "unix:") )
        {
            if ( bind[5] == 0 )
                continue;

            new_sockets = _eventd_sockets_get_unix_sockets(self, bind+5, take_over_socket);
        }
        else if ( g_str_has_prefix(bind, "unix-runtime:") )
        {
            if ( bind[13] == 0 )
                continue;

            gchar *path;

            path = g_build_filename(runtime_dir, bind+13, NULL);
            new_sockets = _eventd_sockets_get_unix_sockets(self, path, take_over_socket);
            g_free(path);
        }
#endif /* G_OS_UNIX */
        sockets = g_list_concat(sockets, new_sockets);
    }

    return sockets;
}

GList *
eventd_sockets_get_sockets(EventdSockets *self, GSocketAddress **binds)
{
    GError *error = NULL;
    GList *list = NULL;

    GSocketAddress **bind_;
    for ( bind_ = binds ; bind_ != NULL ; ++bind_ )
    {
        GSocketAddress *bind = *bind_;
        gssize l;

        l = g_socket_address_get_native_size(bind);
        guchar native_bind[l];
        guchar native_address[l];

        if ( ! g_socket_address_to_native(bind, native_bind, l, &error) )
        {
            g_warning("Couldn't get native sockes address: %s", error->message);
            g_clear_error(&error);
            continue;
        }

        GList *socket_ = self->list, *next_;
        while ( socket_ != NULL )
        {
            next_ = g_list_next(socket_);
            if ( g_socket_get_family(socket_->data) != g_socket_address_get_family(bind) )
                goto next;

            GSocketAddress *address;
            address = g_socket_get_local_address(socket_->data, &error);
            if ( address == NULL )
            {
                g_warning("Couldn't get socket local address: %s", error->message);
                g_clear_error(&error);
                goto next;
            }

            if ( g_socket_address_get_native_size(address) != l )
                goto next;

            if ( ! g_socket_address_to_native(address, native_address, l, &error) )
            {
                g_warning("Couldn't get native sockes address: %s", error->message);
                g_clear_error(&error);
                goto next;
            }

            if ( memcmp(native_bind, native_address, l) == 0 )
            {
                list = g_list_concat(socket_, list);
                self->list = g_list_remove_link(self->list, socket_);
            }
        next:
            socket_ = next_;
        }
    }

    return list;
}

EventdSockets *
eventd_sockets_new(const gchar *runtime_dir, gboolean take_over_socket)
{
    EventdSockets *sockets;

    sockets = g_new0(EventdSockets, 1);

#ifdef ENABLE_SYSTEMD
    gint systemd_fds;
    systemd_fds = sd_listen_fds(TRUE);
    if ( systemd_fds < 0 )
        g_warning("Failed to acquire systemd sockets: %s", g_strerror(-systemd_fds));

    GError *error = NULL;
    gint fd;
    for ( fd = SD_LISTEN_FDS_START ; fd < SD_LISTEN_FDS_START + systemd_fds ; ++fd )
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
