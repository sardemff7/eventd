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

#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#if HAVE_GIO_UNIX
#include <gio/gunixsocketaddress.h>
#endif /* HAVE_GIO_UNIX */

static GSocketConnection *
_eventd_eventdctl_get_connection(const gchar *private_socket, GError **error)
{
    GSocketAddress *address;
    GSocketClient *client;
    GSocketConnection *connection;

#if HAVE_GIO_UNIX
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

    return connection;
}

static int
_eventd_eventdctl_send_command(GIOStream *connection, const gchar *command)
{
    int retval = 0;
    GError *error = NULL;

    if ( ! g_output_stream_write_all(g_io_stream_get_output_stream(connection), command, strlen(command) + 1, NULL, NULL, &error) )
    {
        g_warning("Couldn’t send command '%s'", command);
        g_clear_error(&error);
        retval = 1;
    }

    return retval;
}

static int
_eventd_eventdctl_process_command(const gchar *private_socket, int argc, gchar *argv[])
{
    if ( argc == 0 )
    {
        g_warning("Missing command");
        return 2;
    }

    GError *error = NULL;
    GSocketConnection *connection;

    connection = _eventd_eventdctl_get_connection(private_socket, &error);

    if ( connection == NULL )
    {
        g_warning("Couldn’t connect to eventd: %s", error->message);
        return 1;
    }

    int retval = 2;

    if ( g_strcmp0(argv[0], "quit") == 0 )
        retval = _eventd_eventdctl_send_command(G_IO_STREAM(connection), "quit");
    else if ( g_strcmp0(argv[0], "reload") == 0 )
        retval =  _eventd_eventdctl_send_command(G_IO_STREAM(connection), "reload");
    else if ( g_strcmp0(argv[0], "notification-daemon") == 0 )
    {
        if ( argc == 1 )
            g_warning("You must specify a target");
        else
        {
            gchar *command = g_strconcat("notification-daemon ", argv[1], NULL);
            retval = _eventd_eventdctl_send_command(G_IO_STREAM(connection), command);
            g_free(command);
        }
    }

    if ( ! g_io_stream_close(G_IO_STREAM(connection), NULL, &error) )
        g_warning("Can't close the stream: %s", error->message);
    g_clear_error(&error);

    return retval;
}

int
main(int argc, char *argv[])
{
    gchar *private_socket = NULL;
    gboolean print_version = FALSE;

    GOptionEntry entries[] =
    {
        { "socket", 's', 0, G_OPTION_ARG_FILENAME, &private_socket, "eventd control socket", "<socket>" },
        { "version", 'V', 0, G_OPTION_ARG_NONE, &print_version, "Print version", NULL },
        { NULL }
    };

    GError *error = NULL;
    GOptionContext *context = NULL;

#if ENABLE_NLS
    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
#endif /* ENABLE_NLS */

    g_type_init();

    context = g_option_context_new("<command> [<command arguments>]- control utility for eventd");
    g_option_context_add_main_entries(context, entries, GETTEXT_PACKAGE);
    if ( ! g_option_context_parse(context, &argc, &argv, &error) )
        g_error("Option parsing failed: %s\n", error->message);
    g_option_context_free(context);

    if ( print_version )
    {
        fprintf(stdout, "eventdctl " PACKAGE_VERSION "\n");
        return 0;
    }

    int retval;

    retval = _eventd_eventdctl_process_command(private_socket, argc-1, argv+1);
    g_free(private_socket);

    return retval;
}
