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

#if ENABLE_GIO_UNIX

#include <glib-object.h>
#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>
#include <glib/gstdio.h>

#include "types.h"

#include "eventd.h"
#include "plugins.h"

#include "control.h"

struct _EventdControl {
    EventdCoreContext *core;
    gchar *socket_path;
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

    if ( ! g_output_stream_close(g_io_stream_get_output_stream(G_IO_STREAM(connection)), NULL, &error) )
        g_warning("Can't close the output stream: %s", error->message);
    g_clear_error(&error);

    input = g_data_input_stream_new(g_io_stream_get_input_stream(G_IO_STREAM(connection)));

    if ( ( line = g_data_input_stream_read_upto(input, "\0", 1, &size, NULL, &error) ) == NULL )
    {
        if ( error != NULL )
            g_warning("Can’t read the command: %s", error->message);
        g_clear_error(&error);
    }
    else
    {
#if DEBUG
        g_debug("Received control command: '%s'", line);
#endif /* DEBUG */

        g_data_input_stream_read_byte(input, NULL, &error);
        if ( error != NULL )
            g_clear_error(&error);
        else if ( g_strcmp0(line, "quit") == 0 )
        {
            eventd_core_quit(control->core);
        }
        else if ( g_strcmp0(line, "reload") == 0 )
        {
            eventd_core_config_reload(control->core);
        }
        else
            eventd_plugins_control_command_all(line);

        g_free(line);
    }

    if ( ! g_io_stream_close(G_IO_STREAM(connection), NULL, &error) )
        g_warning("Can't close the stream: %s", error->message);
    g_clear_error(&error);

    return TRUE;
}

EventdControl *
eventd_control_new(EventdCoreContext *core)
{
    EventdControl *control;

    control = g_new0(EventdControl, 1);

    control->core = core;

    return control;
}

void
eventd_control_free(EventdControl *control)
{
    g_free(control);
}

void
eventd_control_start(EventdControl *control)
{
    GError *error = NULL;
    GSocket *socket;

    control->socket_service = g_threaded_socket_service_new(-1);

    if ( ( control->socket_path != NULL ) && ( *control->socket_path == 0 ) )
    {
        g_free(control->socket_path);
        control->socket_path = NULL;
    }

    socket = eventd_core_get_unix_socket(control->core, control->socket_path, "private");
    g_free(control->socket_path);
    if ( socket == NULL )
        return;

    if ( ! g_socket_listener_add_socket(G_SOCKET_LISTENER(control->socket_service), socket, NULL, &error) )
        g_warning("Unable to add private socket: %s", error->message);
    g_clear_error(&error);
    g_object_unref(socket);

    g_signal_connect(control->socket_service, "run", G_CALLBACK(_eventd_service_private_connection_handler), control);
}

void
eventd_control_stop(EventdControl *control)
{
    g_socket_service_stop(control->socket_service);
    g_socket_listener_close(G_SOCKET_LISTENER(control->socket_service));
    g_object_unref(control->socket_service);
}

void
eventd_control_add_option_entry(EventdControl *control, GOptionGroup *option_group)
{
    GOptionEntry entries[] =
    {
        { "private-socket", 'i', 0, G_OPTION_ARG_FILENAME, &control->socket_path, "UNIX socket to listen for internal control", "<socket>" },
        { NULL }
    };
    g_option_group_add_entries(option_group, entries);
}

#else /* ! ENABLE_GIO_UNIX */

#include "types.h"

#include "control.h"

EventdControl *eventd_control_start(EventdCoreContext *service) { return NULL; }
void eventd_control_free(EventdControl *control) {}

void eventd_control_start(EventdControl *control) {}
void eventd_control_stop(EventdControl *control) {}

void eventd_control_add_option_entry(EventdControl *control, GOptionGroup *option_group) {}

#endif /* ! ENABLE_GIO_UNIX */
