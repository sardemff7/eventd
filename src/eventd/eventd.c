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
#include <string.h>
#include <stdio.h>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include "service.h"

#if ENABLE_SYSTEMD
#include <sys/socket.h>
#include <systemd/sd-daemon.h>
#endif /* ENABLE_SYSTEMD */

#if ENABLE_GIO_UNIX
#define MIN_SOCKETS 2
#else /* ! ENABLE_GIO_UNIX */
#define MIN_SOCKETS 1
#endif /* ! ENABLE_GIO_UNIX */

#if ENABLE_SYSTEMD
static GList *
_eventd_get_systemd_sockets()
{
    GError *error = NULL;
    GSocket *socket = NULL;
    GList *sockets = NULL;
    gint r, n;
    gint fd;

    n = sd_listen_fds(TRUE);
    if ( n < 0 )
    {
        g_warning("Failed to acquire systemd socket: %s", strerror(-n));
        return NULL;
    }

    if ( n <= 0 )
    {
        g_warning("No socket received.");
        return NULL;
    }

    for ( fd = SD_LISTEN_FDS_START ; fd < SD_LISTEN_FDS_START + n ; ++fd )
    {
        r = sd_is_socket(fd, AF_UNSPEC, SOCK_STREAM, 1);
        if ( r < 0 )
        {
            g_warning("Failed to verify systemd socket type: %s", strerror(-r));
            return NULL;
        }


        if ( r <= 0 )
        {
            g_warning("Passed socket has wrong type.");
            return NULL;
        }
    }

    for ( fd = SD_LISTEN_FDS_START ; fd < SD_LISTEN_FDS_START + n ; ++fd )
    {
        if ( ( socket = g_socket_new_from_fd(fd, &error) ) == NULL )
        {
            g_warning("Failed to take a socket from systemd: %s", error->message);
            continue;
        }
        sockets = g_list_prepend(sockets, socket);
    }

    return sockets;
}
#endif /* ENABLE_SYSTEMD */


int
main(int argc, char *argv[])
{
    gboolean no_plugins = FALSE;
    gboolean daemonize = FALSE;

    guint16 bind_port = DEFAULT_BIND_PORT;

#ifdef ENABLE_GIO_UNIX
    gboolean no_network = FALSE;
    gboolean no_unix = FALSE;
    gchar *private_socket = NULL;
    gchar *unix_socket = NULL;
    gboolean take_over_socket = FALSE;
#endif /* ENABLE_GIO_UNIX */

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
    gchar *run_dir = NULL;
    GOptionContext *context = NULL;
    GSocket *socket = NULL;
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
        sockets = _eventd_get_systemd_sockets();
    else
#endif /* ENABLE_SYSTEMD */
    {
#if ENABLE_GIO_UNIX
        run_dir = g_build_filename(g_get_user_runtime_dir(), PACKAGE_NAME, NULL);
        if ( g_mkdir_with_parents(run_dir, 0755) < 0 )
        {
            g_warning("Couldn’t create the run dir '%s': %s", run_dir, strerror(errno));
            goto no_run_dir;
        }

        if ( private_socket == NULL )
            private_socket = g_build_filename(run_dir, "private", NULL);
        if ( ( socket = eventd_get_unix_socket(private_socket, take_over_socket) ) != NULL )
            sockets = g_list_prepend(sockets, socket);
        else
            g_warning("Couldn’t create private socket");

        if ( no_unix )
        {
            g_free(unix_socket);
            unix_socket = NULL;
        }
        else
        {
            if ( unix_socket == NULL )
                unix_socket = g_build_filename(run_dir, UNIX_SOCKET, NULL);

            if ( ( socket = eventd_get_unix_socket(unix_socket, take_over_socket) ) != NULL )
                sockets = g_list_prepend(sockets, socket);
            else
            {
                g_free(unix_socket);
                unix_socket = NULL;
            }
        }

    no_run_dir:
        g_free(run_dir);

        if ( no_network )
            bind_port = 0;
#endif /* ENABLE_GIO_UNIX */

        if ( ( bind_port > 0 ) && ( ( socket = eventd_get_inet_socket(bind_port) ) != NULL ) )
            sockets = g_list_prepend(sockets, socket);
    }

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
    g_list_free_full(sockets, g_object_unref);

#if ENABLE_GIO_UNIX
#if ENABLE_SYSTEMD
    if ( no_systemd && unix_socket )
#else /* ! ENABLE_SYSTEMD */
    if ( unix_socket )
#endif /* ! ENABLE_SYSTEMD */
    {
        g_unlink(unix_socket);
        g_free(unix_socket);

        g_unlink(private_socket);
        g_free(private_socket);
    }
#endif /* ENABLE_GIO_UNIX */

    return retval;
}
