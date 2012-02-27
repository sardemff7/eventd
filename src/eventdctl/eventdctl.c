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

#if ENABLE_FRAMEBUFFER
#include <unistd.h>
#endif /* ENABLE_FRAMEBUFFER */

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>
#include <glib/gstdio.h>

static void
_eventd_eventdctl_send_command(GDataOutputStream *output, const gchar *command)
{
    GError *error = NULL;

    if ( ! g_data_output_stream_put_string(output, command, NULL, &error) )
        g_warning("Couldn’t send command '%s'", command);
    g_clear_error(&error);
    if ( ! g_data_output_stream_put_byte(output, '\0', NULL, &error) )
        g_warning("Couldn’t send command '%s'", command);
    g_clear_error(&error);
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

    int retval = 0;
    GError *error = NULL;
    GOptionContext *context = NULL;
    GSocketAddress *address = NULL;
    GSocketClient *client = NULL;
    GSocketConnection *connection = NULL;

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

    if ( private_socket == NULL )
        private_socket = g_build_filename(g_get_user_runtime_dir(), PACKAGE_NAME, "private", NULL);

    if ( ( ! g_file_test(private_socket, G_FILE_TEST_EXISTS) ) || g_file_test(private_socket, G_FILE_TEST_IS_DIR|G_FILE_TEST_IS_REGULAR) )
    {
        g_free(private_socket);
        return 1;
    }

    address = g_unix_socket_address_new(private_socket);
    g_free(private_socket);

    client = g_socket_client_new();
    connection = g_socket_client_connect(client, G_SOCKET_CONNECTABLE(address), NULL, &error);
    g_object_unref(address);

    if ( connection == NULL )
    {
        g_warning("Couldn’t connect to eventd: %s", error->message);
    }
    else if ( argc < 2 )
    {
        g_warning("Missing command");
        retval = 2;
    }
    else
    {
        GDataOutputStream *output = NULL;

        if ( ! g_input_stream_close(g_io_stream_get_input_stream(G_IO_STREAM(connection)), NULL, &error) )
            g_warning("Can't close the input stream: %s", error->message);
        g_clear_error(&error);

        output = g_data_output_stream_new(g_io_stream_get_output_stream(G_IO_STREAM(connection)));

        if ( g_strcmp0(argv[1], "quit") == 0 )
            _eventd_eventdctl_send_command(output, "quit");
        else if ( g_strcmp0(argv[1], "reload") == 0 )
            _eventd_eventdctl_send_command(output, "reload");
        else if ( g_strcmp0(argv[1], "notification-daemon") == 0 )
        {
            if ( argc > 2 )
            {
                gchar *command = g_strconcat("notification-daemon ", argv[2], NULL);
                _eventd_eventdctl_send_command(output, command);
                g_free(command);
            }
            else
            {
                g_warning("You must specify a target");
                retval = 2;
            }
        }

        if ( ! g_io_stream_close(G_IO_STREAM(connection), NULL, &error) )
            g_warning("Can't close the stream: %s", error->message);
        g_clear_error(&error);
    }

    g_object_unref(client);

    return retval;
}
