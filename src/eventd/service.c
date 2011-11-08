/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011 Quentin "Sardem FF7" Glidic
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

#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <signal.h>
#include <errno.h>

#include <glib.h>
#include <gio/gio.h>
#if ENABLE_GIO_UNIX
#include <gio/gunixsocketaddress.h>
#include <glib/gstdio.h>
#endif /* ENABLE_GIO_UNIX */

#include <eventd-plugin.h>
#include "config.h"
#include "plugins.h"
#include "service.h"

#define DEFAULT_DELAY 5
#define DEFAULT_MAX_CLIENTS 5

static void
send_data(const gchar *name, const gchar *content, GDataOutputStream *output)
{
        #if DEBUG
        g_debug("Send back data: %s", name);
        #endif /* DEBUG */

    if ( g_utf8_strchr(content, -1, '\n') == NULL )
    {
        gchar *msg;
        msg = g_strconcat("DATAL ", name, " ", content, "\n", NULL);
        g_data_output_stream_put_string(output, msg, NULL, NULL);
        g_free(msg);
    }
    else
    {
        gchar *msg;
        gchar **line;
        gchar **lines;

        msg = g_strconcat("DATA ", name, "\n", NULL);
        g_data_output_stream_put_string(output, msg, NULL, NULL);

        lines = g_strsplit(content, "\n", 0);

        for ( line = lines ; line != NULL ; ++line )
        {
            if ( (*line)[0] == '.' )
                g_data_output_stream_put_byte(output, '.', NULL, NULL);
            g_data_output_stream_put_string(output, *line, NULL, NULL);
            g_data_output_stream_put_byte(output, '\n', NULL, NULL);
        }

        g_strfreev(lines);

        g_data_output_stream_put_string(output, ".\n", NULL, NULL);
    }
}

static gboolean
connection_handler(
    GThreadedSocketService *service,
    GSocketConnection      *connection,
    GObject                *source_object,
    gpointer                user_data)
{
    GDataInputStream *input = NULL;
    GDataOutputStream *output = NULL;
    GError *error = NULL;

    EventdClient client = {
        .mode = EVENTD_CLIENT_MODE_UNKNOWN
    };
    EventdEvent event = {
        .client = &client
    };

    gchar *event_data_name = NULL;
    gchar *event_data_content = NULL;

    gsize size = 0;
    gchar *line = NULL;

    gint64 delay = 0;
    gint64 last_action = 0;

    input = g_data_input_stream_new(g_io_stream_get_input_stream((GIOStream *)connection));
    output = g_data_output_stream_new(g_io_stream_get_output_stream((GIOStream *)connection));
    delay = eventd_config_get_guint64("delay", DEFAULT_DELAY) * 1e6;

    while ( ( line = g_data_input_stream_read_upto(input, "\n", -1, &size, NULL, &error) ) != NULL )
    {
        g_data_input_stream_read_byte(input, NULL, &error);
        if ( error )
            break;
        #if DEBUG
        g_debug("Line received: %s", line);
        #endif /* DEBUG */

        if ( event.type != NULL )
        {
            if ( g_ascii_strcasecmp(line, ".") == 0 )
            {
                if ( event_data_name )
                {
                    g_hash_table_insert(event.data, event_data_name, event_data_content);
                    event_data_name = NULL;
                    event_data_content = NULL;
                }
                else
                {
                    gint64 event_time = 0;

                    event_time = g_get_monotonic_time();
                    if ( event_time > ( last_action + delay ) )
                    {
                        GHashTable *ret = NULL;

                        last_action = event_time;
                        ret = eventd_plugins_event_action_all(&event);
                        switch ( client.mode )
                        {
                        case EVENTD_CLIENT_MODE_PING_PONG:
                            if ( ! g_data_output_stream_put_string(output, "EVENT\n", NULL, &error) )
                                break;
                            if ( ret != NULL )
                                g_hash_table_foreach(ret, (GHFunc)send_data, output);
                            if ( ! g_data_output_stream_put_string(output, ".\n", NULL, &error) )
                                break;
                        case EVENTD_CLIENT_MODE_NORMAL:
                        default:
                        break;
                        }
                        if ( ret != NULL )
                            g_hash_table_unref(ret);
                    }
                    event.data = (g_hash_table_unref(event.data), NULL);
                    event.type = (g_free(event.type), NULL);
                }
            }
            else if ( event_data_content )
            {
                gchar *old = NULL;

                old = event_data_content;
                event_data_content = g_strjoin("\n", old, ( line[0] == '.' ) ? line+1 : line, NULL);

                g_free(old);
            }
            else if ( g_ascii_strncasecmp(line, "DATA ", 5) == 0 )
            {
                event_data_name = g_strdup(line+5);
            }
            else if ( g_ascii_strncasecmp(line, "DATAL ", 6) == 0 )
            {
                gchar **data = NULL;

                data = g_strsplit(line+6, " ", 2);
                g_hash_table_insert(event.data, g_strdup(data[0]), g_strdup(data[1]));

                g_strfreev(data);
            }
            else
                event_data_content = g_strdup(( line[0] == '.' ) ? line+1 : line);
        }
        else if ( g_ascii_strncasecmp(line, "EVENT ", 6) == 0 )
        {
            event.type = g_strdup(line+6);
            event.data = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        }
        else if ( g_ascii_strcasecmp(line, "BYE") == 0 )
        {
            g_data_output_stream_put_string(output, "BYE\n", NULL, &error);
            break;
        }
        else if ( g_ascii_strncasecmp(line, "MODE ", 5) == 0 )
        {
            EventdClientMode mode;

            if ( last_action > 0 )
            {
                g_warning("Can’t change the mode after the first event");
                g_free(line);
                continue;
            }

            if ( ! g_data_output_stream_put_string(output, "MODE\n", NULL, &error) )
                break;

            if ( g_ascii_strcasecmp(line+5, "normal") == 0 )
                mode = EVENTD_CLIENT_MODE_NORMAL;
            else if ( g_ascii_strcasecmp(line+5, "ping-pong") == 0 )
                mode = EVENTD_CLIENT_MODE_PING_PONG;
            else
            {
                mode = EVENTD_CLIENT_MODE_UNKNOWN;
                g_warning("Unknown mode");
            }

            client.mode = mode;
        }
        else if ( g_ascii_strncasecmp(line, "HELLO ", 6) == 0 )
        {
            gchar **hello = NULL;

            if ( ! g_data_output_stream_put_string(output, "HELLO\n", NULL, &error) )
                break;

            hello = g_strsplit(line+6, " ", 2);
            client.type = g_strdup(hello[0]);
            if ( hello[1] != NULL )
                client.name = g_strdup(hello[1]);
            g_strfreev(hello);
        }
        else if ( g_ascii_strncasecmp(line, "RENAME ", 7) == 0 )
        {
            gchar **rename = NULL;

            if ( ! g_data_output_stream_put_string(output, "RENAMED\n", NULL, &error) )
                break;

            rename = g_strsplit(line+7, " ", 2);
            g_free(client.type);
            g_free(client.name);
            client.type = g_strdup(rename[0]);
            if ( rename[1] != NULL )
                client.name = g_strdup(rename[1]);
            g_strfreev(rename);
        }
        else
            g_warning("Unknown message");

        g_free(line);
    }
    if ( error )
        g_warning("Can't read the socket: %s", error->message);
    g_clear_error(&error);

    g_free(client.type);
    g_free(client.name);

    if ( ! g_io_stream_close((GIOStream *)connection, NULL, &error) )
        g_warning("Can't close the stream: %s", error->message);
    g_clear_error(&error);

    return TRUE;
}

static GMainLoop *loop = NULL;

static void
sig_quit_handler(int sig)
{
    if ( loop )
        g_main_loop_quit(loop);
    else
        exit(1);
}


GSocket *
eventd_get_inet_socket(guint16 port)
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

#if ENABLE_GIO_UNIX
GSocket *
eventd_get_unix_socket(gchar *path, gboolean take_over_socket)
{
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
    if ( socket )
        g_object_unref(socket);
    g_clear_error(&error);
    return NULL;
}
#endif /* ENABLE_GIO_UNIX */

int
#if DEBUG
eventd_service(GList *sockets, gboolean no_plugins)
#else /* ! DEBUG */
eventd_service(GList *sockets)
#endif /* ! DEBUG */
{
    int retval = 0;
    GError *error = NULL;
    GList *socket = NULL;
    GSocketService *service = NULL;

    signal(SIGTERM, sig_quit_handler);
    signal(SIGINT, sig_quit_handler);

    #if DEBUG
    if ( ! no_plugins )
    #endif /* DEBUG */
    eventd_plugins_load();

    eventd_config_parser();

    service = g_threaded_socket_service_new(eventd_config_get_gint64("max-clients", DEFAULT_MAX_CLIENTS));

    for ( socket = g_list_first(sockets) ; socket ; socket = g_list_next(socket) )
    {
        if ( ! g_socket_listener_add_socket((GSocketListener *)service, socket->data, NULL, &error) )
            g_warning("Unable to add socket: %s", error->message);
        g_clear_error(&error);
    }

    g_signal_connect(G_OBJECT(service), "run", G_CALLBACK(connection_handler), NULL);

    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    loop = NULL;

    g_socket_service_stop(service);
    g_socket_listener_close((GSocketListener *)service);

    eventd_config_clean();

    eventd_plugins_unload();

    return retval;
}
