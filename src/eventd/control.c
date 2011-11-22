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

#if ENABLE_GIO_UNIX

#include <glib-object.h>
#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>
#include <glib/gstdio.h>

#include "types.h"

#include "service.h"

#include "control.h"

struct _EventdControl {
    EventdService *service;
    GSocketService *socket_service;
};


static gboolean
_eventd_service_private_connection_handler(GThreadedSocketService *socket_service, GSocketConnection *connection, GObject *source_object, gpointer user_data)
{
    EventdControl *control = user_data;

    GError *error = NULL;
    GDataInputStream *input = NULL;
    gsize size = 0;
    gchar *line = NULL;

    if ( ! g_output_stream_close(g_io_stream_get_output_stream((GIOStream *)connection), NULL, &error) )
        g_warning("Can't close the output stream: %s", error->message);
    g_clear_error(&error);

    input = g_data_input_stream_new(g_io_stream_get_input_stream((GIOStream *)connection));

    if ( ( line = g_data_input_stream_read_upto(input, "\0", 1, &size, NULL, &error) ) == NULL )
    {
        if ( error != NULL )
            g_warning("Can’t read the command: %s", error->message);
        g_clear_error(&error);
    }
    else
    {

        g_data_input_stream_read_byte(input, NULL, &error);
        if ( error != NULL )
            g_clear_error(&error);
        else if ( g_strcmp0(line, "quit") == 0 )
        {
            eventd_service_quit(control->service);
        }
        else if ( g_strcmp0(line, "reload") == 0 )
        {
            eventd_service_config_reload(control->service);
        }
    }

    if ( ! g_input_stream_close((GInputStream *)input, NULL, &error) )
        g_warning("Can't close the input stream: %s", error->message);
    g_clear_error(&error);

    if ( ! g_io_stream_close((GIOStream *)connection, NULL, &error) )
        g_warning("Can't close the stream: %s", error->message);
    g_clear_error(&error);

    return TRUE;
}

EventdControl *
eventd_control_start(gpointer service, GList **sockets)
{
    GError *error = NULL;
    GList *socket = NULL;
    EventdControl *control;

    control = g_new0(EventdControl, 1);

    control->service = service;

    control->socket_service = g_threaded_socket_service_new(-1);

    socket = g_list_last(*sockets);

    *sockets = g_list_remove_link(*sockets, socket);
    if ( ! g_socket_listener_add_socket((GSocketListener *)control->socket_service, socket->data, NULL, &error) )
        g_warning("Unable to add private socket: %s", error->message);
    g_clear_error(&error);
    g_list_free_full(socket, g_object_unref);

    g_signal_connect(control->socket_service, "run", G_CALLBACK(_eventd_service_private_connection_handler), control);

    return control;
}

void
eventd_control_stop(EventdControl *control)
{
    g_socket_service_stop(control->socket_service);
    g_socket_listener_close((GSocketListener *)control->socket_service);

    g_free(control);
}

#else /* ! ENABLE_GIO_UNIX */

#include "control.h"

EventdControl *eventd_control_start(gpointer service, GList **sockets) { return NULL; }
void eventd_control_stop(EventdControl *control) {}

#endif /* ! ENABLE_GIO_UNIX */
