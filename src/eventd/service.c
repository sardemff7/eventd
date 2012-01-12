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

#include <glib.h>
#include <glib-object.h>
#ifdef G_OS_UNIX
#include <glib-unix.h>
#endif /* G_OS_UNIX */
#include <gio/gio.h>
#if ENABLE_GIO_UNIX
#include <gio/gunixsocketaddress.h>
#include <glib/gstdio.h>
#endif /* ENABLE_GIO_UNIX */

#include <libeventd-event.h>

#include "types.h"

#include "config.h"
#include "plugins.h"
#include "control.h"
#include "queue.h"
#include "dbus.h"

#include "service.h"

struct _EventdService {
    EventdControl *control;
    EventdDbusContext *dbus;
    EventdConfig *config;
    EventdQueue *queue;
    GMainLoop *loop;
    GSocketService *service;
    GSList *clients;
};

static void
_eventd_service_client_disconnect(gpointer data)
{
    GCancellable *cancellable = data;
    g_cancellable_cancel(cancellable);
    g_object_unref(cancellable);
}

void
eventd_service_config_reload(gpointer user_data)
{
    EventdService *service = user_data;
    service->config = eventd_config_parser(service->config);
}

gboolean
eventd_service_quit(gpointer user_data)
{
    EventdService *service = user_data;

    eventd_queue_free(service->queue);

    g_slist_free_full(service->clients, _eventd_service_client_disconnect);

    if ( service->loop != NULL )
        g_main_loop_quit(service->loop);

    return FALSE;
}

static void
_eventd_service_send_data(gpointer key, gpointer value, gpointer user_data)
{
    const gchar *name = key;
    const gchar *content = value;
    GDataOutputStream *output = user_data;

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

typedef enum {
    MODE_NORMAL = 0,
    MODE_RELAY,
    MODE_PING_PONG,
} EventdClientMode;

static gboolean
_eventd_service_connection_handler(GThreadedSocketService *socket_service, GSocketConnection *connection, GObject *source_object, gpointer user_data)
{
    EventdService *service = user_data;

    GCancellable *cancellable;
    GDataInputStream *input = NULL;
    GDataOutputStream *output = NULL;
    GError *error = NULL;

    gchar *category = NULL;
    gchar *client_name = NULL;
    EventdEvent *event = NULL;

    EventdClientMode mode = MODE_NORMAL;

    gchar *event_data_name = NULL;
    gchar *event_data_content = NULL;

    gsize size = 0;
    gchar *line = NULL;

    gint32 last_eventd_id = 0;

    cancellable = g_cancellable_new();

    service->clients = g_slist_prepend(service->clients, cancellable);

    input = g_data_input_stream_new(g_io_stream_get_input_stream((GIOStream *)connection));
    output = g_data_output_stream_new(g_io_stream_get_output_stream((GIOStream *)connection));

    while ( ( line = g_data_input_stream_read_upto(input, "\n", -1, &size, cancellable, &error) ) != NULL )
    {
        g_data_input_stream_read_byte(input, NULL, &error);
        if ( error )
            break;
        #if DEBUG
        g_debug("Line received: %s", line);
        #endif /* DEBUG */

        if ( event != NULL )
        {
            if ( g_ascii_strcasecmp(line, ".") == 0 )
            {
                if ( event_data_name )
                {
                    eventd_event_add_data(event, event_data_name, event_data_content);
                    event_data_name = NULL;
                    event_data_content = NULL;
                }
                else if ( ( mode == MODE_RELAY )
                          && ( eventd_event_get_category(event) == NULL ) )
                {
                    g_warning("Relay client missing real client description");
                    g_object_unref(event);
                    event = NULL;
                }
                else
                {
                    gboolean disable;
                    gint64 timeout;

                    eventd_config_event_get_disable_and_timeout(service->config, event, &disable, &timeout);
                    eventd_event_set_timeout(event, timeout);

                    if ( ! disable )
                    {
                        GHashTable *pong = NULL;

                        switch ( mode )
                        {
                        case MODE_NORMAL:
                        case MODE_RELAY:
                            eventd_queue_push(service->queue, event);
                        break;
                        case MODE_PING_PONG:
                            eventd_plugins_event_pong_all(event);
                            if ( ! g_data_output_stream_put_string(output, "EVENT\n", NULL, &error) )
                                break;
                            pong = eventd_event_get_pong_data(event);
                            if ( pong != NULL )
                                g_hash_table_foreach(pong, _eventd_service_send_data, output);
                            if ( ! g_data_output_stream_put_string(output, ".\n", NULL, &error) )
                                break;
                            eventd_event_end(event, EVENTD_EVENT_END_REASON_RESERVED);
                        break;
                        }
                        if ( error != NULL )
                            break;
                    }

                    g_object_unref(event);
                    event = NULL;
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
                eventd_event_add_data(event, data[0], data[1]);
                g_free(data);
            }
            else if ( ( mode == MODE_RELAY )
                      && ( g_ascii_strncasecmp(line, "CLIENT ", 7) == 0 ) )
            {
                gchar **relay_client = NULL;

                relay_client = g_strsplit(line+7, " ", 2);

                eventd_event_set_category(event, relay_client[0]);
                if ( relay_client[1][0] != 0 )
                    eventd_event_add_data(event, g_strdup("client-name"), g_strdup(relay_client[1]));

                g_strfreev(relay_client);
            }
            else if ( event_data_name )
                event_data_content = g_strdup(( line[0] == '.' ) ? line+1 : line);
            else
                g_warning("Unknown message");
        }
        else if ( g_ascii_strncasecmp(line, "EVENT ", 6) == 0 )
        {
            if ( category == NULL )
                break;

            last_eventd_id = eventd_queue_get_next_event_id(service->queue);

            event = eventd_event_new_with_id(last_eventd_id, line+6);

            if ( mode != MODE_RELAY )
            {
                eventd_event_set_category(event, category);
                eventd_event_add_data(event, g_strdup("client-name"), g_strdup(client_name));
            }
        }
        else if ( g_ascii_strcasecmp(line, "BYE") == 0 )
        {
            if ( category == NULL )
                break;

            g_data_output_stream_put_string(output, "BYE\n", NULL, &error);
            break;
        }
        else if ( g_ascii_strncasecmp(line, "MODE ", 5) == 0 )
        {
            if ( category == NULL )
                break;

            if ( last_eventd_id > 0 )
            {
                g_warning("Can’t change the mode after the first event");
                g_free(line);
                continue;
            }

            if ( ! g_data_output_stream_put_string(output, "MODE\n", NULL, &error) )
                break;

            if ( g_ascii_strcasecmp(line+5, "relay") == 0 )
                mode = MODE_RELAY;
            else if ( g_ascii_strcasecmp(line+5, "ping-pong") == 0 )
                mode = MODE_PING_PONG;
        }
        else if ( g_ascii_strncasecmp(line, "HELLO ", 6) == 0 )
        {
            gchar **hello = NULL;

            if ( ! g_data_output_stream_put_string(output, "HELLO\n", NULL, &error) )
                break;

            hello = g_strsplit(line+6, " ", 2);

            category = hello[0];
            client_name = hello[1];

            g_free(hello);
        }
        else if ( g_ascii_strncasecmp(line, "RENAME ", 7) == 0 )
        {
            gchar **rename = NULL;

            if ( category == NULL )
                break;

            if ( ! g_data_output_stream_put_string(output, "RENAMED\n", NULL, &error) )
                break;

            rename = g_strsplit(line+7, " ", 2);

            g_free(category);
            g_free(client_name);
            category = rename[0];
            client_name = rename[1];

            g_free(rename);
        }
        else
            g_warning("Unknown message");

        g_free(line);
    }
    if ( ( error != NULL ) && ( error->code == G_IO_ERROR_CANCELLED ) )
        g_warning("Can't read the socket: %s", error->message);
    g_clear_error(&error);

    g_free(category);
    g_free(client_name);

    if ( ! g_io_stream_close((GIOStream *)connection, NULL, &error) )
        g_warning("Can't close the stream: %s", error->message);
    g_clear_error(&error);

    return TRUE;
}

int
eventd_service(GList *sockets, gboolean no_plugins)
{
    int retval = 0;
    GError *error = NULL;
    GList *socket = NULL;
    EventdService *service;

    service = g_new0(EventdService, 1);

#ifdef G_OS_UNIX
    g_unix_signal_add(SIGTERM, eventd_service_quit, service);
    g_unix_signal_add(SIGINT, eventd_service_quit, service);
#endif /* G_OS_UNIX */

    service->control = eventd_control_start(service, &sockets);

    if ( ! no_plugins )
        eventd_plugins_load();

    service->config = eventd_config_parser(service->config);

    service->queue = eventd_queue_new(service);

    service->service = g_threaded_socket_service_new(eventd_config_get_max_clients(service->config));

    for ( socket = g_list_first(sockets) ; socket ; socket = g_list_next(socket) )
    {
        if ( ! g_socket_listener_add_socket((GSocketListener *)service->service, socket->data, NULL, &error) )
            g_warning("Unable to add socket: %s", error->message);
        g_clear_error(&error);
    }

    g_signal_connect(service->service, "run", G_CALLBACK(_eventd_service_connection_handler), service);

    service->dbus = eventd_dbus_start(service->config, service->queue);

    service->loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(service->loop);
    g_main_loop_unref(service->loop);

    service->loop = NULL;

    eventd_dbus_stop(service->dbus);

    g_socket_service_stop(service->service);
    g_socket_listener_close((GSocketListener *)service->service);

    eventd_config_clean(service->config);

    eventd_control_stop(service->control);

    eventd_plugins_unload();

    return retval;
}
