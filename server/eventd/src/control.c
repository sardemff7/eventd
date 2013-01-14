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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "types.h"

#include "eventd.h"
#include "plugins.h"

#include "control.h"

struct _EventdControl {
    EventdCoreContext *core;
    gchar *socket;
    GSocketService *socket_service;
};

static gboolean
_eventd_service_private_connection_handler(GSocketService *socket_service, GSocketConnection *connection, GObject *source_object, gpointer user_data)
{
    EventdControl *control = user_data;
    GIOStream *stream = G_IO_STREAM(connection);

    GError *error = NULL;
    GDataInputStream *input = NULL;
    gsize size = 0;
    gchar *line = NULL;

    input = g_data_input_stream_new(g_io_stream_get_input_stream(stream));

    if ( ( line = g_data_input_stream_read_upto(input, "\0", 1, &size, NULL, &error) ) == NULL )
    {
        if ( error != NULL )
            g_warning("Couldn't read the command: %s", error->message);
        g_clear_error(&error);
    }
    else
    {
        const gchar *answer = "Done";

#ifdef DEBUG
        g_debug("Received control command: '%s'", line);
#endif /* DEBUG */

        g_data_input_stream_read_byte(input, NULL, &error);
        if ( error != NULL )
            g_clear_error(&error);
        else if ( g_strcmp0(line, "stop") == 0 )
        {
            eventd_core_stop(control->core);
        }
        else if ( g_strcmp0(line, "reload") == 0 )
        {
            eventd_core_config_reload(control->core);
        }
        else if ( g_strcmp0(line, "pause") == 0 )
        {
            eventd_core_pause(control->core);
        }
        else if ( g_strcmp0(line, "resume") == 0 )
        {
            eventd_core_resume(control->core);
        }
        else if ( g_str_has_prefix(line, "add-flag ") )
        {
            eventd_core_add_flag(control->core, g_quark_from_string(line + strlen("add-flag ")));
        }
        else if ( g_strcmp0(line, "reset-flags") == 0 )
        {
            eventd_core_reset_flags(control->core);
        }
        else
            eventd_plugins_control_command_all(line);

        g_free(line);

        if ( ! g_output_stream_write_all(g_io_stream_get_output_stream(stream), answer, strlen(answer) + 1, NULL, NULL, &error) )
            g_warning("Couldn't send answer '%s': %s", answer, error->message);
        g_clear_error(&error);
    }

    g_object_unref(input);

    if ( ! g_io_stream_close(stream, NULL, &error) )
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

gboolean
eventd_control_start(EventdControl *control)
{
    gboolean ret = FALSE;
    GError *error = NULL;
    GList *sockets = NULL;
    GList *socket_;

    control->socket_service = g_socket_service_new();

    const gchar *binds[] = { NULL, NULL };

    if ( control->socket != NULL )
    {
        gchar *bind;

#ifdef HAVE_GIO_UNIX
        bind = g_strconcat("unix:", control->socket, NULL);
#else /* ! HAVE_GIO_UNIX */
        bind = g_strconcat("tcp:localhost:", control->socket, NULL);
#endif /* ! HAVE_GIO_UNIX */
        binds[0] = bind;
        sockets = eventd_core_get_sockets(control->core, binds);

        g_free(bind);
        g_free(control->socket);
    }

    if ( sockets == NULL )
    {
#ifdef HAVE_GIO_UNIX
        binds[0] = "unix-runtime:private";
#else /* ! HAVE_GIO_UNIX */
        binds[0] = "tcp:localhost:" DEFAULT_CONTROL_PORT_STR;
#endif /* ! HAVE_GIO_UNIX */
        sockets = eventd_core_get_sockets(control->core, binds);
    }

    if ( sockets == NULL )
    {
        g_warning("No control socket available, stopping");
        return ret;
    }

    for ( socket_ = sockets ; socket_ != NULL ; socket_ = g_list_next(socket_) )
    {
        if ( ! g_socket_listener_add_socket(G_SOCKET_LISTENER(control->socket_service), socket_->data, NULL, &error) )
            g_warning("Unable to add private socket: %s", error->message);
        else
            ret = TRUE;
        g_clear_error(&error);
    }
    g_list_free_full(sockets, g_object_unref);

    if ( ret )
        g_signal_connect(control->socket_service, "incoming", G_CALLBACK(_eventd_service_private_connection_handler), control);
    else
       eventd_control_stop(control);

    return ret;
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
        { "private-socket", 'i', 0, G_OPTION_ARG_FILENAME, &control->socket, "socket to listen for internal control", "<socket>" },
        { NULL }
    };
    g_option_group_add_entries(option_group, entries);
}
