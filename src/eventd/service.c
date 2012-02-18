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

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#if ENABLE_GIO_UNIX
#include <glib/gstdio.h>
#endif /* ENABLE_GIO_UNIX */

#include <libeventd-event.h>

#include "types.h"

#include "eventd.h"
#include "config.h"
#include "plugins.h"
#include "avahi.h"

#include "service.h"

struct _EventdService {
    EventdCoreContext *core;
    EventdAvahiContext *avahi;
    EventdConfig *config;
    guint16 bind_port;
    gchar *unix_socket;
    gboolean no_avahi;
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
    EventdEvent *event = NULL;

    EventdClientMode mode = MODE_NORMAL;

    gchar *event_data_name = NULL;
    gchar *event_data_content = NULL;

    gsize size = 0;
    gchar *line = NULL;

    gint32 last_eventd_id = 0;

    cancellable = g_cancellable_new();

    service->clients = g_slist_prepend(service->clients, cancellable);

    input = g_data_input_stream_new(g_io_stream_get_input_stream(G_IO_STREAM(connection)));
    output = g_data_output_stream_new(g_io_stream_get_output_stream(G_IO_STREAM(connection)));

    while ( ( line = g_data_input_stream_read_upto(input, "\n", -1, &size, cancellable, &error) ) != NULL )
    {
        g_data_input_stream_read_byte(input, NULL, &error);
        if ( error != NULL )
            break;
#if DEBUG
        g_debug("Line received: %s", line);
#endif /* DEBUG */

        if ( event != NULL )
        {
            if ( g_ascii_strcasecmp(line, ".") == 0 )
            {
                if ( event_data_name != NULL )
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

                    if ( ! eventd_config_event_get_disable(service->config, event) )
                    {
                        GHashTable *pong = NULL;

                        switch ( mode )
                        {
                        case MODE_NORMAL:
                        case MODE_RELAY:
                            eventd_core_push_event(service->core, event);
                        break;
                        case MODE_PING_PONG:
                            eventd_core_event_pong(service->core, event);
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
            else if ( event_data_content != NULL )
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
                eventd_event_set_category(event, line+7);
            else if ( event_data_name != NULL )
                event_data_content = g_strdup(( line[0] == '.' ) ? line+1 : line);
            else
                g_warning("Unknown message");
        }
        else if ( g_ascii_strncasecmp(line, "EVENT ", 6) == 0 )
        {
            if ( category == NULL )
                break;

            event = eventd_event_new(line+6);

            if ( mode != MODE_RELAY )
                eventd_event_set_category(event, category);
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
            if ( ! g_data_output_stream_put_string(output, "HELLO\n", NULL, &error) )
                break;

            category = g_strdup(line+6);
        }
        else if ( g_ascii_strncasecmp(line, "RENAME ", 7) == 0 )
        {
            if ( category == NULL )
                break;

            if ( ! g_data_output_stream_put_string(output, "RENAMED\n", NULL, &error) )
                break;

            g_free(category);
            category = g_strdup(line+7);
        }
        else
            g_warning("Unknown message");

        g_free(line);
    }
    if ( ( error != NULL ) && ( error->code == G_IO_ERROR_CANCELLED ) )
        g_warning("Can't read the socket: %s", error->message);
    g_clear_error(&error);

    g_free(category);

    if ( ! g_io_stream_close(G_IO_STREAM(connection), NULL, &error) )
        g_warning("Can't close the stream: %s", error->message);
    g_clear_error(&error);

    return TRUE;
}

EventdService *
eventd_service_new(EventdCoreContext *core, EventdConfig *config)
{
    EventdService *service;

    service = g_new0(EventdService, 1);

    service->core = core;
    service->config = config;

    service->bind_port = DEFAULT_BIND_PORT;

    return service;
}

void
eventd_service_free(EventdService *service)
{
    g_free(service->unix_socket);

    g_free(service);
}

void
eventd_service_start(EventdService *service)
{
    GError *error = NULL;
    GList *sockets = NULL;
    GSocket *socket = NULL;
#if ENABLE_GIO_UNIX
    gchar *used_path = NULL;
    gboolean created = FALSE;
#endif /* ENABLE_GIO_UNIX */

    service->service = g_threaded_socket_service_new(eventd_config_get_max_clients(service->config));

    if ( service->bind_port > 0 )
    {
        socket = eventd_core_get_inet_socket(service->core, service->bind_port);
        sockets = g_list_prepend(sockets, socket);
    }
    if ( socket != NULL )
    {
        if ( ! g_socket_listener_add_socket(G_SOCKET_LISTENER(service->service), socket, NULL, &error) )
            g_warning("Unable to add socket: %s", error->message);
        g_clear_error(&error);
    }

#if ENABLE_GIO_UNIX
    if ( ( service->unix_socket != NULL ) && ( *service->unix_socket == 0 ) )
    {
        socket = NULL;
    }
    else
        socket = eventd_core_get_unix_socket(service->core, service->unix_socket, UNIX_SOCKET, &used_path, &created);
    if ( used_path != NULL )
    {
        g_free(service->unix_socket);
        service->unix_socket = used_path;
    }
    if ( ! created )
    {
        g_free(service->unix_socket);
        service->unix_socket = NULL;
    }
    if ( socket != NULL )
    {
        sockets = g_list_prepend(sockets, socket);
        if ( ! g_socket_listener_add_socket(G_SOCKET_LISTENER(service->service), socket, NULL, &error) )
            g_warning("Unable to add socket: %s", error->message);
        g_clear_error(&error);
    }
#endif /* ENABLE_GIO_UNIX */

    g_signal_connect(service->service, "run", G_CALLBACK(_eventd_service_connection_handler), service);

    if ( ! service->no_avahi )
        service->avahi = eventd_avahi_start(service->config, sockets);
}

void
eventd_service_stop(EventdService *service)
{
    eventd_avahi_stop(service->avahi);

    g_slist_free_full(service->clients, _eventd_service_client_disconnect);

    g_socket_service_stop(service->service);
    g_socket_listener_close(G_SOCKET_LISTENER(service->service));
    g_object_unref(service->service);

#if ENABLE_GIO_UNIX
    if ( service->unix_socket != NULL )
        g_unlink(service->unix_socket);
#endif /* ENABLE_GIO_UNIX */
}

GOptionGroup *
eventd_service_get_option_group(EventdService *context)
{
    GOptionGroup *option_group;
    GOptionEntry entries[] =
    {
        { "port", 'p', 0, G_OPTION_ARG_INT, &context->bind_port, "Port to listen for inbound connections", "<port>" },
#if ENABLE_GIO_UNIX
        { "socket", 's', 0, G_OPTION_ARG_FILENAME, &context->unix_socket, "UNIX socket to listen for inbound connections", "<socket>" },
#endif /* ENABLE_GIO_UNIX */
#if ENABLE_AVAHI
        { "no-avahi", 'A', 0, G_OPTION_ARG_NONE, &context->no_avahi, "Disable avahi publishing", NULL },
#endif /* ENABLE_AVAHI */
        { NULL }
    };

    option_group = g_option_group_new("event", "EVENT protocol plugin options", "Show EVENT plugin help options", NULL, NULL);
    g_option_group_set_translation_domain(option_group, GETTEXT_PACKAGE);
    g_option_group_add_entries(option_group, entries);
    return option_group;
}
