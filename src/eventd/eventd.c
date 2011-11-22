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
#include <stdio.h>
#include <sys/file.h>
#include <unistd.h>

#include <glib.h>
#include <glib-object.h>

#include "sockets.h"
#include "service.h"

int
main(int argc, char *argv[])
{
    gboolean no_plugins = FALSE;
    gboolean daemonize = FALSE;

    guint16 bind_port = DEFAULT_BIND_PORT;

    gboolean no_network = FALSE;
    gboolean no_unix = FALSE;
    gchar *private_socket = NULL;
    gchar *unix_socket = NULL;
    gboolean take_over_socket = FALSE;

#ifdef ENABLE_SYSTEMD
    gboolean no_systemd = FALSE;
#endif /* ENABLE_SYSTEMD */

    GOptionEntry entries[] =
    {
#ifdef DEBUG
        { "no-plugins", 'P', 0, G_OPTION_ARG_NONE, &no_plugins, "Disable systemd socket activation", NULL },
#else /* ! DEBUG */
        { "daemonize", 'd', 0, G_OPTION_ARG_NONE, &daemonize, "Run the daemon in the background", NULL },
#endif /* ! DEBUG */
        { "port", 'p', 0, G_OPTION_ARG_INT, &bind_port, "Port to listen for inbound connections", "P" },
#if ENABLE_GIO_UNIX
        { "no-network", 'N', 0, G_OPTION_ARG_NONE, &no_network, "Disable the network bind", NULL },
        { "no-unix", 'U', 0, G_OPTION_ARG_NONE, &no_unix, "Disable the UNIX socket bind", NULL },
        { "private-socket", 'i', 0, G_OPTION_ARG_FILENAME, &private_socket, "UNIX socket to listen for internal control", "SOCKET_FILE" },
        { "socket", 's', 0, G_OPTION_ARG_FILENAME, &unix_socket, "UNIX socket to listen for inbound connections", "SOCKET_FILE" },
        { "take-over", 't', 0, G_OPTION_ARG_NONE, &take_over_socket, "Take over socket", NULL },
#endif /* ENABLE_GIO_UNIX */
#ifdef ENABLE_SYSTEMD
        { "no-systemd", 'S', 0, G_OPTION_ARG_NONE, &no_systemd, "Disable systemd socket activation", NULL },
#endif /* ENABLE_SYSTEMD */
        { NULL }
    };

    int retval = 0;
    GError *error = NULL;
    GOptionContext *context = NULL;
    GList *sockets = NULL;

#ifdef ENABLE_NLS
    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
#endif /* ENABLE_NLS */

    g_type_init();

    context = g_option_context_new("- small daemon to act on remote or local events");
    g_option_context_add_main_entries(context, entries, GETTEXT_PACKAGE);
    if ( ! g_option_context_parse(context, &argc, &argv, &error) )
        g_error("Option parsing failed: %s\n", error->message);
    g_option_context_free(context);

#if ENABLE_SYSTEMD
    if ( ! no_systemd )
        sockets = eventd_sockets_get_systemd(&private_socket, &unix_socket);
    else
#endif /* ENABLE_SYSTEMD */
        sockets = eventd_sockets_get_all(bind_port, no_network, no_unix, &private_socket, &unix_socket, take_over_socket);

    if ( g_list_length(sockets) < MIN_SOCKETS )
    {
        g_warning("Nothing to bind to, kind of useless, isn't it?");
        retval = 2;
    }
    else
    {
        if ( daemonize )
        {
            pid_t pid = -1;

            pid = fork();
            if ( pid == -1 )
            {
                perror("fork");
                return 1;
            }
            else if ( pid != 0 )
                return 0;
            close(0);
            close(1);
            close(2);
            open("/dev/null", O_RDWR);
            dup2(0,1);
            dup2(0,2);
        }

        retval = eventd_service(sockets, no_plugins);
    }

    eventd_sockets_free_all(sockets, unix_socket, private_socket);

    return retval;
}
