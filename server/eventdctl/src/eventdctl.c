/*
 * eventdctl - Control utility for eventd
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
#include <glib/gstdio.h>
#include <gio/gio.h>
#ifdef HAVE_GIO_UNIX
#include <gio/gunixsocketaddress.h>
#endif /* HAVE_GIO_UNIX */

#include <eventdctl.h>

static gboolean
_eventd_eventdctl_start_eventd(int argc, gchar *argv[], GError **error)
{
    gboolean retval = FALSE;
    GSpawnFlags flags = 0;
    GPid pid;
    gint stdin_fd;

    gchar **args;
    gchar **current;

    args = g_new(gchar *, argc+3);
    current = args;

    if ( ( argc > 0 ) && ( g_strcmp0(argv[0], "--argv0") == 0 ) )
    {
        ++argv;
        --argc;
    }
    else
    {
        flags |= G_SPAWN_SEARCH_PATH;
        *current = "eventd";
        ++current;
    }

    int i;
    for ( i = 0 ; i < argc ; ++i, ++current )
        *current = argv[i];
    *current = "--daemonize";
    ++current;
    *current = NULL;

    if ( ! g_spawn_async_with_pipes(NULL, args, NULL, flags, NULL, NULL, &pid, NULL, &stdin_fd, NULL, error) )
        goto fail;

    GIOChannel *stdin_io;

#ifdef G_OS_UNIX
    stdin_io = g_io_channel_unix_new(stdin_fd);
#else /* ! G_OS_UNIX */
    stdin_io = g_io_channel_win32_new_fd(stdin_fd);
#endif /* ! G_OS_UNIX */

    gchar *data;
    gsize length;

    if ( g_io_channel_read_to_end(stdin_io, &data, &length, error) == G_IO_STATUS_NORMAL )
        g_free(data);
    g_free(stdin_io);

    retval = TRUE;

fail:
    g_free(args);
    return retval;
}

static GIOStream *
_eventd_eventdctl_get_connection(const gchar *private_socket, GError **error)
{
    GSocketAddress *address;
    GSocketClient *client;
    GSocketConnection *connection;

#ifdef HAVE_GIO_UNIX
    const gchar *real_socket = private_socket;
    gchar *default_socket = NULL;
    if ( private_socket == NULL )
        real_socket = default_socket = g_build_filename(g_get_user_runtime_dir(), PACKAGE_NAME, "private", NULL);

    if ( ( ! g_file_test(real_socket, G_FILE_TEST_EXISTS) ) || g_file_test(real_socket, G_FILE_TEST_IS_DIR|G_FILE_TEST_IS_REGULAR) )
    {
        g_free(default_socket);
        return NULL;
    }

    address = g_unix_socket_address_new(real_socket);
    g_free(default_socket);
#else /* ! HAVE_GIO_UNIX */
    GInetAddress *inet_address;
    guint16 port = DEFAULT_CONTROL_PORT;
    inet_address = g_inet_address_new_loopback(G_SOCKET_FAMILY_IPV6);
    if ( private_socket != NULL )
    {
        guint64 parsed_port;
        parsed_port = g_ascii_strtoull(private_socket, NULL, 10);
        port = CLAMP(parsed_port, 1, 65535);
    }
    address = g_inet_socket_address_new(inet_address, port);
    g_object_unref(inet_address);
#endif /* ! HAVE_GIO_UNIX */

    client = g_socket_client_new();
    connection = g_socket_client_connect(client, G_SOCKET_CONNECTABLE(address), NULL, error);
    g_object_unref(address);
    g_object_unref(client);

    return G_IO_STREAM(connection);
}

static EventdctlReturnCode
_eventd_eventdctl_send_argv(GIOStream *connection, gint argc, gchar *argv[])
{
    EventdctlReturnCode retval = EVENTDCTL_RETURN_CODE_CONNECTION_ERROR;
    GError *error = NULL;

    GDataOutputStream *output;
    GDataInputStream *input;


    output = g_data_output_stream_new(g_io_stream_get_output_stream(connection));
    input = g_data_input_stream_new(g_io_stream_get_input_stream(connection));

    if ( ! g_data_output_stream_put_uint64(output, argc, NULL, &error) )
    {
        g_warning("Couldn't send argc: %s", error->message);
        goto fail;
    }
    gint i;
    for ( i = 0 ; i < argc ; ++i )
    {
        if ( ! g_data_output_stream_put_string(output, argv[i], NULL, &error) )
        {
            g_warning("Couldn't send argv[%d] '%s': %s", i, argv[i], error->message);
            goto fail;
        }
        if ( ! g_data_output_stream_put_byte(output, '\0', NULL, &error) )
        {
            g_warning("Couldn't send argv[%d] '%s' NUL byte: %s", i, argv[i], error->message);
            goto fail;
        }

    }

    EventdctlReturnCode r;
    gchar *status;
    r = g_data_input_stream_read_uint64(input, NULL, &error);
    if ( error != NULL )
    {
        g_warning("Couldn't read the return code: %s", error->message);
        goto fail;
    }
    status = g_data_input_stream_read_upto(input, "\0", 1, NULL, NULL, &error);
    if ( error != NULL )
    {
        g_warning("Couldn't read the status message: %s", error->message);
        goto fail;
    }

    retval = r;
    g_printf("%s\n", status);
    g_free(status);

fail:
    g_object_unref(input);
    g_object_unref(output);
    g_clear_error(&error);
    return retval;
}

static EventdctlReturnCode
_eventd_eventdctl_process_command(const gchar *private_socket, gboolean autospawn, int argc, gchar *argv[])
{
    GError *error = NULL;
    GIOStream *connection;

    connection = _eventd_eventdctl_get_connection(private_socket, &error);

    EventdctlReturnCode retval = EVENTDCTL_RETURN_CODE_OK;

    if ( g_strcmp0(argv[0], "start") == 0 )
    {
        if ( connection != NULL )
            goto close;
        g_clear_error(&error);
        if ( ! _eventd_eventdctl_start_eventd(argc-1, argv+1, &error) )
        {
            g_warning("Couldn't start eventd: %s", error->message);
            return EVENTDCTL_RETURN_CODE_INVOCATION_ERROR;
        }
        connection = _eventd_eventdctl_get_connection(private_socket, &error);
        if ( connection != NULL )
            goto close;
    }

    if ( ( connection == NULL ) && autospawn )
    {
        int a_argc = 0;
        gchar *a_argv[2];
        if ( private_socket != NULL )
        {
            a_argc = 2;
            a_argv[0] = "--private-socket";
            a_argv[1] = (gchar *)private_socket;
        }
        if ( ! _eventd_eventdctl_start_eventd(a_argc, a_argv, &error) )
        {
            g_warning("Couldn't start eventd: %s", error->message);
            return EVENTDCTL_RETURN_CODE_INVOCATION_ERROR;
        }
        connection = _eventd_eventdctl_get_connection(private_socket, &error);
    }

    if ( connection == NULL )
    {
        if ( error != NULL )
            g_warning("Couldn't connect to eventd: %s", error->message);
        return EVENTDCTL_RETURN_CODE_CONNECTION_ERROR;
    }

    gchar *plugin = NULL;
    if ( g_strcmp0(argv[0], "notification-daemon") == 0 )
        argv[0] = "eventd-nd";
    else if (
                 ( g_strcmp0(argv[0], "evp") == 0 )
              || ( g_strcmp0(argv[0], "dbus") == 0 )
              || ( g_strcmp0(argv[0], "relay") == 0 )
              || ( g_strcmp0(argv[0], "exec") == 0 )
              || ( g_strcmp0(argv[0], "nd") == 0 )
              || ( g_strcmp0(argv[0], "sndfile") == 0 )
              || ( g_strcmp0(argv[0], "espeak") == 0 )
              || ( g_strcmp0(argv[0], "notify") == 0 )
            )
    {

        plugin = g_strconcat("eventd-", argv[0], NULL);
        argv[0] = plugin;
    }

    retval = _eventd_eventdctl_send_argv(connection, argc, argv);

    g_free(plugin);
close:
    if ( ! g_io_stream_close(connection, NULL, &error) )
        g_warning("Can't close the stream: %s", error->message);
    g_clear_error(&error);

    return retval;
}

int
main(int argc, char *argv[])
{
    gchar *private_socket = NULL;
    gboolean autospawn = FALSE;
    gboolean print_version = FALSE;

    GOptionEntry entries[] =
    {
        { "socket",     's', 0, G_OPTION_ARG_FILENAME, &private_socket, "eventd control socket",  "<socket>" },
        { "auto-spawn", 'a', 0, G_OPTION_ARG_NONE,     &autospawn,      "Spawn eventd if needed", NULL },
        { "version",    'V', 0, G_OPTION_ARG_NONE,     &print_version,  "Print version",          NULL },
        { NULL }
    };

#ifdef ENABLE_NLS
    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
#endif /* ENABLE_NLS */

#if ! GLIB_CHECK_VERSION(2,35,1)
    g_type_init();
#endif /* ! GLIB_CHECK_VERSION(2,35,1) */

    GError *error = NULL;
    GOptionContext *context;

    context = g_option_context_new("<command> [<command arguments>] - control utility for eventd");
    g_option_context_add_main_entries(context, entries, GETTEXT_PACKAGE);
    g_option_context_set_ignore_unknown_options(context, TRUE);
    if ( ! g_option_context_parse(context, &argc, &argv, &error) )
        g_error("Option parsing failed: %s\n", error->message);
    g_option_context_free(context);

    if ( print_version )
    {
        fprintf(stdout, "eventdctl " PACKAGE_VERSION "\n");
        return EVENTDCTL_RETURN_CODE_OK;
    }

    EventdctlReturnCode retval;

    retval = _eventd_eventdctl_process_command(private_socket, autospawn, argc-1, argv+1);
    g_free(private_socket);

    return retval;
}
