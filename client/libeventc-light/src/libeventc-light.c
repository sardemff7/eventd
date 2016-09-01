/*
 * libeventc-light - Library to communicate with eventd, light (local-only non-GIO) version
 *
 * Copyright © 2011-2016 Quentin "Sardem FF7" Glidic
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with eventd. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif /* HAVE_FCNTL_H */
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif /* HAVE_SYS_SOCKET_H */
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif /* HAVE_SYS_UN_H */
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */

#ifdef G_OS_WIN32
#ifndef UNICODE
#define UNICODE 1
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define errno WSAGetLastError()
#define close(s) closesocket(s)
#endif /* G_OS_WIN32 */

#include <glib.h>

#include <libeventd-event.h>
#include <libeventd-protocol.h>

#include <libeventc-light.h>

struct _EventcLightConnection {
    guint64 refcount;
    gchar *name;
    gboolean subscribe;
    GHashTable *subscriptions;
    struct {
        EventcLightConnectionEventCallback callback;
        gpointer user_data;
        GDestroyNotify notify;
    } event_callback;
    EventdProtocol *protocol;
    EventcLightSocket socket;
    struct {
        gchar *buffer;
        gsize length;
    } buffer;
};

static void _eventc_light_connection_close_internal(EventcLightConnection *self);

/**
 * eventc_light_get_version:
 *
 * Retrieves the runtime-version of libeventc.
 *
 * Returns: (transfer none): the version of libeventc
 */
EVENTD_EXPORT
const gchar *
eventc_light_get_version(void)
{
    return EVENTD_VERSION;
}

static gint
_eventc_light_connection_send_message(EventcLightConnection *self, gchar *message)
{
#ifdef EVENTD_DEBUG
    g_debug("Sending message:\n%s", message);
#endif /* EVENTD_DEBUG */

    gint error = 0;

    gsize o = 0;
    gsize length;
    length = strlen(message);
    while ( ( error == 0 ) && ( o < length ) )
    {
        gssize r;
        r = send(self->socket, message+o, length-o, 0);
        if ( r < 0 )
            error = -errno;
        else
            o += r;
    }

    g_free(message);
    return error;
}

static void
_eventc_light_connection_protocol_event(EventdProtocol *protocol, EventdEvent *event, gpointer user_data)
{
    EventcLightConnection *self = user_data;

    if ( self->event_callback.callback != NULL )
        self->event_callback.callback(self, event, self->event_callback.user_data);
}

static void
_eventc_light_connection_protocol_bye(EventdProtocol *protocol, const gchar *message, gpointer user_data)
{
    EventcLightConnection *self = user_data;

    _eventc_light_connection_close_internal(self);
}

static const EventdProtocolCallbacks _eventc_light_connection_protocol_callbacks = {
    .event = _eventc_light_connection_protocol_event,
    .bye = _eventc_light_connection_protocol_bye,
};

/**
 * eventc_light_connection_new:
 * @name: (nullable): the host running the eventd instance to connect to or %NULL (equivalent to "localhost")
 *
 * Creates a new connection to an eventd daemon.
 * See eventc_light_connection_set_name() for the exact format of @name.
 *
 * Returns: (transfer full): a new connection
 */
EVENTD_EXPORT
EventcLightConnection *
eventc_light_connection_new(const gchar *name)
{
#ifdef G_OS_WIN32
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
#endif /* G_OS_WIN32 */

    EventcLightConnection *self;

    self = g_new0(EventcLightConnection, 1);
    self->refcount = 1;

    self->name = g_strdup(name);

    self->protocol = eventd_protocol_new(&_eventc_light_connection_protocol_callbacks, self, NULL);

    return self;
}

static void
_eventc_light_connection_free(EventcLightConnection *self)
{
    if ( self->subscriptions != NULL )
        g_hash_table_unref(self->subscriptions);

    eventd_protocol_unref(self->protocol);

    g_free(self);

#ifdef G_OS_WIN32
    WSACleanup();
#endif /* G_OS_WIN32 */
}

/**
 * eventc_light_connection_ref:
 * @connection: an #EventcLightConnection
 *
 * Increments the reference counter of @connection.
 *
 * Returns: (transfer full): the #EventdEvent
 */
EVENTD_EXPORT
EventcLightConnection *
eventc_light_connection_ref(EventcLightConnection *self)
{
    g_return_val_if_fail(self != NULL, NULL);

    ++self->refcount;

    return self;
}

/**
 * eventc_light_connection_unref:
 * @connection: an #EventcLightConnection
 *
 * Decrements the reference counter of @connection.
 * If it reaches 0, free @connection.
 */
EVENTD_EXPORT
void
eventc_light_connection_unref(EventcLightConnection *self)
{
    g_return_if_fail(self != NULL);

    if ( --self->refcount < 1 )
        _eventc_light_connection_free(self);
}

/**
 * eventc_light_connection_set_event_callback:
 * @connection: an #EventcLightConnection
 * @callback: (scope notified) (closure user_data) (destroy notify): The event callback
 *
 * Sets the callback to be called when receiving an event.
 */
EVENTD_EXPORT
void
eventc_light_connection_set_event_callback(EventcLightConnection *self, EventcLightConnectionEventCallback callback, gpointer user_data, GDestroyNotify notify)
{
    g_return_if_fail(self != NULL);
    g_return_if_fail(callback != NULL);

    if ( self->event_callback.notify != NULL )
        self->event_callback.notify(self->event_callback.user_data);

    self->event_callback.callback = callback;
    self->event_callback.user_data = user_data;
    self->event_callback.notify = notify;
}

/**
 * eventc_light_connection_is_connected:
 * @connection: an #EventcLightConnection
 * @error: (out) (optional): return location for %errno value or %NULL to ignore
 *
 * Retrieves whether a given connection is actually connected to a server or
 * not.
 *
 * Returns: %TRUE if the connection was successful
 */
EVENTD_EXPORT
gboolean
eventc_light_connection_is_connected(EventcLightConnection *self, gint *error)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == 0, FALSE);

    return ( self->socket != 0 );
}

static gboolean
_eventc_light_connection_expect_connected(EventcLightConnection *self, gint *error)
{
    if ( eventc_light_connection_is_connected(self, error) )
        return TRUE;

    if ( *error == 0 )
        *error = -ENOTCONN;

    return FALSE;
}

static gboolean
_eventc_light_connection_expect_disconnected(EventcLightConnection *self, gint *error)
{
    if ( ( ! eventc_light_connection_is_connected(self, error) ) && ( *error == 0 ) )
        return TRUE;

    if ( *error == 0 )
        *error = -EISCONN;

    return FALSE;
}

/**
 * eventc_light_connection_connect:
 * @connection: an #EventcLightConnection
 *
 * Initializes the connection to the stored name.
 *
 * This call does bloking I/O.
 */
EVENTD_EXPORT
gint
eventc_light_connection_connect(EventcLightConnection *self)
{
    g_return_val_if_fail(self != NULL, -EFAULT);

    gint error = 0;
    if ( ! _eventc_light_connection_expect_disconnected(self, &error) )
        return error;

    const gchar *name = self->name;
    gchar *name_ = NULL;

    if ( ( name == NULL ) || ( *name == '\0' ) )
        name = g_getenv("EVENTC_HOST");

    if ( ( name == NULL ) || ( *name == '\0' ) || ( g_strcmp0(name, "localhost") == 0 ) )
    {
        const gchar *runtime_dir = g_get_user_runtime_dir();
#ifdef G_OS_UNIX
        /* System mode */
        if ( g_getenv("XDG_RUNTIME_DIR") == NULL )
            runtime_dir = "/run";
#endif /* G_OS_UNIX */
        name = name_ = g_build_filename(runtime_dir, PACKAGE_NAME, EVP_UNIX_SOCKET, NULL);
    }

    if ( ! g_path_is_absolute(name) )
    {
        error = -EINVAL;
        goto ret;
    }

#ifdef G_OS_UNIX
    gsize length;
    length = strlen(name);
    sa_family_t family = AF_UNIX;
    struct sockaddr_un addr = {
        .sun_family = family
    };

    if ( ( length + 1 ) > sizeof(addr.sun_path) )
    {
        error = -E2BIG;
        goto ret;
    }

    strncpy(addr.sun_path, name, length+1);
    if ( g_str_has_prefix(name, "@") )
        addr.sun_path[0] = '\0';

    gint fd;

    fd = socket(family, SOCK_STREAM, 0);
    if ( fd < 0 )
    {
        error = -errno;
        goto ret;
    }

    if ( connect(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0 )
    {
        error = -errno;
        close(fd);
        goto ret;
    }

    if ( fcntl(fd, F_SETFL, O_NONBLOCK) < 0 )
    {
        error = -errno;
        close(fd);
        goto ret;
    }

    self->socket = fd;
#else /* ! G_OS_UNIX */
    if ( ! g_file_test(name, G_FILE_TEST_IS_REGULAR) )
    {
        error = -ENOENT;
        goto ret;
    }

    GError *_inner_error_ = NULL;
    gchar *str;
    guint64 port;
    g_file_get_contents(name, &str, NULL, &_inner_error_);
    if ( str == NULL )
    {
        error = -WSAEINVAL;
        goto ret;
    }

    port = g_ascii_strtoull(str, NULL, 10);
    //g_free(str);
    if ( ( port == 0 ) || ( port > G_MAXUINT16 ) )
    {
        error = -ERANGE;
        goto ret;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
        .sin_port = htons(port)
    };

    SOCKET sock;

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if ( sock == INVALID_SOCKET )
    {
        error = -WSAGetLastError();
        goto ret;
    }

    g_debug("Socket created, connecting to [%s]:%llu", inet_ntoa(addr.sin_addr), port);

    if ( connect(sock, (SOCKADDR *) &addr, sizeof(addr)) == SOCKET_ERROR )
    {
        error = -WSAGetLastError();
        closesocket(sock);
        goto ret;
    }
    g_debug("Socket connected");

    gulong set = 1;
    if ( ioctlsocket(sock, FIONBIO, &set) == SOCKET_ERROR )
    {
        error = -WSAGetLastError();
        closesocket(sock);
        goto ret;
    }
    g_debug("Socket non blocking");

    self->socket = sock;
#endif /* ! G_OS_UNIX */

    if ( self->subscribe )
        error = _eventc_light_connection_send_message(self, eventd_protocol_generate_subscribe(self->protocol, self->subscriptions));

    if ( error != 0 )
    {
        close(self->socket);
        self->socket = 0;
    }
    g_debug("ok");

ret:
    g_free(name_);
    return error;
}

/**
 * eventc_light_connection_get_socket:
 * @connection: an #EventcLightConnection
 *
 * Retrieves the connection socket.
 *
 * Returns: the connection socket
 */
EVENTD_EXPORT
EventcLightSocket
eventc_light_connection_get_socket(EventcLightConnection *self)
{
    return self->socket;
}

/**
 * eventc_light_connection_read:
 * @connection: an #EventcLightConnection
 *
 * Reads the incoming data on connection socket.
 *
 * Returns: 0 if the read was successful, a negative %errno value otherwise
 */
EVENTD_EXPORT
gint
eventc_light_connection_read(EventcLightConnection *self)
{
    g_return_val_if_fail(self != NULL, -EFAULT);

    gint error = 0;
    if ( ! _eventc_light_connection_expect_connected(self, &error) )
        return error;

    gchar buf[4096];
    gchar *buffer = self->buffer.buffer;
    gsize length = self->buffer.length;
    gsize o = length;

    gssize r;
    while ( ( r = recv(self->socket, buf, sizeof(buf), 0) ) > 0 )
    {
        length += r;
        buffer = g_realloc(buffer, length);
        strncpy(buffer+o, buf, r);
        buffer[length] = '\0';
        o = length;
    }
    if ( r == 0 )
    {
        close(self->socket);
        self->socket = 0;
        error = 1;
    }
    else if ( ( errno != EAGAIN ) && ( errno != EWOULDBLOCK ) )
        error = -errno;
    else
    {
        GError *error = NULL;
        o = 0;
        gchar *c;
        while ( ( c = g_utf8_strchr(buffer + o, length - o, '\n') ) != NULL )
        {
            *c = '\0';
            if ( ! eventd_protocol_parse(self->protocol, buffer + o, &error) )
            {
                g_error_free(error);
                return -EINVAL;
            }
            o = c + 1 - buffer;
        }
        self->buffer.length = length - o;
        self->buffer.buffer = g_strndup(buffer + o, self->buffer.length);

        g_free(buffer);
    }

    return error;
}

/**
 * eventc_light_connection_event:
 * @connection: an #EventcLightConnection
 * @event: an #EventdEvent to send to the server
 *
 * Sends an event across the connection.
 *
 * Returns: 0 if the event was sent successfully, a negative %errno value otherwise
 */
EVENTD_EXPORT
gint
eventc_light_connection_event(EventcLightConnection *self, EventdEvent *event)
{
    g_return_val_if_fail(self != NULL, -EFAULT);
    g_return_val_if_fail(event != NULL, -EFAULT);

    gint error = 0;
    if ( ! _eventc_light_connection_expect_connected(self, &error) )
        return error;

    return _eventc_light_connection_send_message(self, eventd_protocol_generate_event(self->protocol, event));
}

/**
 * eventc_light_connection_close:
 * @connection: an #EventcLightConnection
 *
 * Closes the connection.
 *
 * Returns: 0 if the connection was successfully closed, a negative %errno value otherwise
 */
EVENTD_EXPORT
gint
eventc_light_connection_close(EventcLightConnection *self)
{
    g_return_val_if_fail(self != NULL, -EFAULT);

    gint error = 0;
    if ( eventc_light_connection_is_connected(self, &error) )
        _eventc_light_connection_send_message(self, eventd_protocol_generate_bye(self->protocol, NULL));
    else if ( error != 0 )
        return error;

    _eventc_light_connection_close_internal(self);

    return 0;
}

static void
_eventc_light_connection_close_internal(EventcLightConnection *self)
{
    close(self->socket);
    self->socket = 0;
}


/**
 * eventc_light_connection_set_name:
 * @connection: an #EventcLightConnection
 * @name: (nullable): the file name to use or "localhost" (default file name) or %NULL (equivalent to "localhost")
 *
 * Sets the file name libeventc-light will use to connect.
 *
 * On UNIX, it is a UNIX socket. You can use abstract sockets by prepending '@' to the name.
 * On Windows, it is a file that contains the port to connect on localhost.
 */
EVENTD_EXPORT
void
eventc_light_connection_set_name(EventcLightConnection *self, const gchar *name)
{
    g_return_if_fail(self != NULL);

    g_free(self->name);
    self->name = g_strdup(name);
}

/**
 * eventc_light_connection_set_subscribe:
 * @connection: an #EventcLightConnection
 * @subscribe: the subscribe setting
 *
 * Sets whether the connection will subscribe to events.
 * If you do not add specific categories using eventc_light_connection_add_subscription(),
 * it will subscribe to *all* events.
 */
EVENTD_EXPORT
void
eventc_light_connection_set_subscribe(EventcLightConnection *self, gboolean subscribe)
{
    g_return_if_fail(self != NULL);

    self->subscribe = subscribe;
}

/**
 * eventc_light_connection_add_subscription:
 * @connection: an #EventcLightConnection
 * @category: (transfer full): a category of events to subscribe to
 *
 * Adds a category of events to subscribe to.
 */
EVENTD_EXPORT
void
eventc_light_connection_add_subscription(EventcLightConnection *self, gchar *category)
{
    g_return_if_fail(self != NULL);
    g_return_if_fail(category != NULL);

    if ( self->subscriptions == NULL )
        self->subscriptions = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    g_hash_table_add(self->subscriptions, category);
}

/**
 * eventc_light_connection_get_subscribe:
 * @connection: an #EventcLightConnection
 *
 * Retrieves whether the connection will subscribe to events.
 *
 * Returns: %TRUE if the connection will subscribe to events
 */
EVENTD_EXPORT
gboolean
eventc_light_connection_get_subscribe(EventcLightConnection *self)
{
    g_return_val_if_fail(self != NULL, FALSE);

    return self->subscribe;
}
