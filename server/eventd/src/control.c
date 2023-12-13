/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2021 Quentin "Sardem FF7" Glidic
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

#include "config.h"

#include <string.h>

#include <glib.h>
#include <glib-object.h>
#ifndef G_OS_UNIX
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#endif /* ! G_OS_UNIX */
#include <gio/gio.h>

#include "nkutils-git-version.h"

#include "eventdctl.h"

#include "types.h"

#include "eventd.h"
#include "plugins.h"

#include "control.h"

#ifdef G_OS_UNIX
#define PRIVATE_SOCKET_BIND_PREFIX "unix"
#else /* ! G_OS_UNIX */
#define PRIVATE_SOCKET_BIND_PREFIX "tcp-file"
#endif /* ! G_OS_UNIX */

struct _EventdControl {
    EventdCoreContext *core;
    GSocketService *socket_service;
};

struct _EventdControlDelayedStop {
    GIOStream *stream;
    GDataOutputStream *output;
};

static void
_eventd_control_send_response(GDataOutputStream *output, EventdctlReturnCode code, const gchar *status)
{
    GError *error = NULL;

    if ( ! g_data_output_stream_put_uint64(output, code, NULL, &error) )
        g_warning("Couldn't send return code '%u': %s", code, error->message);
    else if ( status != NULL )
    {

        if ( ! g_data_output_stream_put_string(output, status, NULL, &error) )
            g_warning("Couldn't send status message '%s': %s", status, error->message);
        else if ( ! g_data_output_stream_put_byte(output, '\0', NULL, &error) )
            g_warning("Couldn't send status message end byte: %s", error->message);
    }
}

static gboolean
_eventd_service_private_connection_handler(GSocketService *socket_service, GSocketConnection *connection, GObject *source_object, gpointer user_data)
{
    EventdControl *control = user_data;
    GIOStream *stream = G_IO_STREAM(connection);

    GError *error = NULL;
    GDataInputStream *input;
    GDataOutputStream *output;

    input = g_data_input_stream_new(g_io_stream_get_input_stream(stream));
    output = g_data_output_stream_new(g_io_stream_get_output_stream(stream));

    guint64 argc, i;
    gchar *arg, **argv = NULL;
    argc = g_data_input_stream_read_uint64(input, NULL, &error);
    if ( error != NULL )
    {
        g_warning("Couldn't read the command argc: %s", error->message);
        goto fail;
    }
    argv = g_new0(char *, argc + 1);
    for ( i = 0 ; i < argc ; ++i )
    {
        arg = g_data_input_stream_read_upto(input, "\0", 1, NULL, NULL, &error);
        g_data_input_stream_read_byte(input, NULL, NULL);
        if ( error != NULL )
        {
            g_warning("Couldn't read the command argv[%" G_GUINT64_FORMAT "]: %s", i, error->message);
            goto fail;
        }
        argv[i] = arg;
    }

    gchar *status = NULL;
    EventdctlReturnCode code = EVENTDCTL_RETURN_CODE_OK;
    if ( argc == 0 )
    {
        status = g_strdup("Missing command");
        code = EVENTDCTL_RETURN_CODE_COMMAND_ERROR;
    }
    else if ( g_strcmp0(argv[0], "start") == 0 )
        /* No-op */;
    else if ( g_strcmp0(argv[0], "stop") == 0 )
    {
        EventdControlDelayedStop *delayed_stop;

        delayed_stop = g_slice_new(EventdControlDelayedStop);
        delayed_stop->stream = g_object_ref(stream);
        delayed_stop->output = output;
        eventd_core_stop(control->core, delayed_stop);
        g_strfreev(argv);
        g_object_unref(input);
        return TRUE;
    }
    else if ( g_strcmp0(argv[0], "reload") == 0 )
        eventd_core_config_reload(control->core);
    else if ( g_strcmp0(argv[0], "version") == 0 )
        status = g_strdup(PACKAGE_NAME " " NK_PACKAGE_VERSION);
    else if ( g_strcmp0(argv[0], "dump") == 0 )
    {
        if ( argc < 2 )
        {
            status = g_strdup("Missing dump command");
            code = EVENTDCTL_RETURN_CODE_COMMAND_ERROR;
        }
        else if ( g_strcmp0(argv[1], "event") == 0 )
        {
            if ( argc < 3 )
            {
                status = g_strdup("Missing event");
                code = EVENTDCTL_RETURN_CODE_COMMAND_ERROR;
            }
            else
            {
                status = eventd_core_dump_event(control->core, argv[2]);
                if ( status == NULL )
                {
                    status = g_strdup_printf("Unknown event '%s'", argv[2]);
                    code = EVENTDCTL_RETURN_CODE_COMMAND_ERROR;
                }
            }
        }
        else if ( g_strcmp0(argv[1], "action") == 0 )
        {
            if ( argc < 3 )
            {
                status = g_strdup("Missing action");
                code = EVENTDCTL_RETURN_CODE_COMMAND_ERROR;
            }
            else
            {
                status = eventd_core_dump_action(control->core, argv[2]);
                if ( status == NULL )
                {
                    status = g_strdup_printf("Unknown action '%s'", argv[2]);
                    code = EVENTDCTL_RETURN_CODE_COMMAND_ERROR;
                }
            }
        }
    }
    else if ( g_strcmp0(argv[0], "flags") == 0 )
    {
        if ( argc < 2 )
        {
            status = g_strdup("Missing flags command");
            code = EVENTDCTL_RETURN_CODE_COMMAND_ERROR;
        }
        else if ( g_strcmp0(argv[1], "add") == 0 )
        {
            if ( argc < 3 )
            {
                status = g_strdup("Missing flag");
                code = EVENTDCTL_RETURN_CODE_COMMAND_ERROR;
            }
            else
                eventd_core_flags_add(control->core, g_quark_from_string(argv[2]));
        }
        else if ( g_strcmp0(argv[1], "remove") == 0 )
        {
            if ( argc < 3 )
            {
                status = g_strdup("Missing flag");
                code = EVENTDCTL_RETURN_CODE_COMMAND_ERROR;
            }
            else
                eventd_core_flags_remove(control->core, g_quark_from_string(argv[2]));
        }
        else if ( g_strcmp0(argv[1], "test") == 0 )
        {
            if ( argc < 3 )
            {
                status = g_strdup("Missing flag");
                code = EVENTDCTL_RETURN_CODE_COMMAND_ERROR;
            }
            else if ( eventd_core_flags_test(control->core, g_quark_from_string(argv[2])))
                status = g_strdup("yes");
            else
            {
                status = g_strdup("no");
                code = EVENTDCTL_RETURN_CODE_PLUGIN_CUSTOM_1;
            }
        }
        else if ( g_strcmp0(argv[1], "reset") == 0 )
            eventd_core_flags_reset(control->core);
        else if ( g_strcmp0(argv[1], "list") == 0 )
            status = eventd_core_flags_list(control->core);
        else
        {
            status = g_strdup_printf("Unknown command '%s'", argv[1]);
            code = EVENTDCTL_RETURN_CODE_COMMAND_ERROR;
        }
    }
    else
    {
        if ( argc < 2 )
        {
            status = g_strdup("No plugin command specified");
            code = EVENTDCTL_RETURN_CODE_COMMAND_ERROR;
        }
        else
            code = eventd_plugins_control_command(argv[0], argc-1, (const gchar * const *)argv+1, &status);
    }

    _eventd_control_send_response(output, code, status);
    g_free(status);

fail:
    g_strfreev(argv);
    g_object_unref(output);
    g_object_unref(input);
    g_clear_error(&error);

    if ( ! g_io_stream_close(stream, NULL, &error) )
        g_warning("Can't close the stream: %s", error->message);
    g_clear_error(&error);

    return TRUE;
}

void
eventd_control_stop(EventdControl *control, EventdControlDelayedStop *delayed_stop)
{
    GError *error = NULL;

    _eventd_control_send_response(delayed_stop->output, EVENTDCTL_RETURN_CODE_OK, NULL);

    g_object_unref(delayed_stop->output);

    if ( ! g_io_stream_close(delayed_stop->stream, NULL, &error) )
        g_warning("Can't close the stream: %s", error->message);
    g_clear_error(&error);

    g_object_unref(delayed_stop->stream);

    g_slice_free(EventdControlDelayedStop, delayed_stop);
}

EventdControl *
eventd_control_new(EventdCoreContext *core, const gchar *control_socket)
{
    gboolean ret = FALSE;
    GSocketService *socket_service;
    GError *error = NULL;
    GList *sockets = NULL;

    socket_service = g_socket_service_new();
    g_socket_service_stop(socket_service);

    const gchar *binds[] = { NULL, NULL };
    gchar *bind = NULL;

    if ( control_socket != NULL )
    {
        bind = g_strconcat(PRIVATE_SOCKET_BIND_PREFIX ":", control_socket, NULL);
        binds[0] = bind;
    }
    else
        binds[0] = PRIVATE_SOCKET_BIND_PREFIX "-runtime:private";

    sockets = eventd_core_get_binds(core, PACKAGE_NAME "-control", binds);

    g_free(bind);

    if ( sockets == NULL )
    {
        g_warning("No control socket available, stopping");
        return NULL;
    }

    ret = TRUE;

    GList *socket_;
    for ( socket_ = sockets ; ( socket_ != NULL ) && ret; socket_ = g_list_next(socket_) )
    {
        GSocket *socket = socket_->data;
        if ( ! g_socket_listener_add_socket(G_SOCKET_LISTENER(socket_service), socket, NULL, &error) )
        {
            g_warning("Unable to add private socket: %s", error->message);
            ret = FALSE;
        }
    }
    g_clear_error(&error);

    g_list_free_full(sockets, g_object_unref);

    if ( ! ret )
    {
        g_object_unref(socket_service);
        return NULL;
    }

    EventdControl *control;

    control = g_new0(EventdControl, 1);

    control->core = core;
    control->socket_service = socket_service;

    g_signal_connect(control->socket_service, "incoming", G_CALLBACK(_eventd_service_private_connection_handler), control);
    g_socket_service_start(control->socket_service);

    return control;
}

void
eventd_control_free(EventdControl *control)
{
    g_socket_service_stop(control->socket_service);
    g_socket_listener_close(G_SOCKET_LISTENER(control->socket_service));
    g_object_unref(control->socket_service);
    g_free(control);
}
