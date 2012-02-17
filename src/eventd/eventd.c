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
#include <glib/gprintf.h>

#include "sockets.h"
#include "service.h"

int
main(int argc, char *argv[])
{
    guint16 bind_port = DEFAULT_BIND_PORT;

    gchar *private_socket = NULL;
    gchar *unix_socket = NULL;
    gboolean take_over_socket = FALSE;

    gboolean no_avahi = FALSE;

    gboolean print_version = FALSE;

    GOptionEntry entries[] =
    {
        { "port", 'p', 0, G_OPTION_ARG_INT, &bind_port, "Port to listen for inbound connections", "P" },
#if ENABLE_GIO_UNIX
        { "private-socket", 'i', 0, G_OPTION_ARG_FILENAME, &private_socket, "UNIX socket to listen for internal control", "SOCKET_FILE" },
        { "socket", 's', 0, G_OPTION_ARG_FILENAME, &unix_socket, "UNIX socket to listen for inbound connections", "SOCKET_FILE" },
        { "take-over", 't', 0, G_OPTION_ARG_NONE, &take_over_socket, "Take over socket", NULL },
#endif /* ENABLE_GIO_UNIX */
#if ENABLE_AVAHI
        { "no-avahi", 'A', 0, G_OPTION_ARG_NONE, &no_avahi, "Disable avahi publishing", NULL },
#endif /* ENABLE_AVAHI */
        { "version", 'V', 0, G_OPTION_ARG_NONE, &print_version, "Print version", NULL },
        { NULL }
    };

    int retval = 0;
    GError *error = NULL;
    GOptionContext *option_context = NULL;
    GList *sockets = NULL;

#if DEBUG
    g_setenv("G_MESSAGES_DEBUG", "all", FALSE);
#endif /* ! DEBUG */

#if ENABLE_NLS
    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
#endif /* ENABLE_NLS */

    g_type_init();

    option_context = g_option_context_new("- small daemon to act on remote or local events");
    g_option_context_add_main_entries(option_context, entries, GETTEXT_PACKAGE);
    if ( ! g_option_context_parse(option_context, &argc, &argv, &error) )
        g_error("Option parsing failed: %s\n", error->message);
    g_option_context_free(option_context);

    if ( print_version )
    {
        g_fprintf(stdout, PACKAGE_NAME " " PACKAGE_VERSION "\n");
        return 0;
    }

    sockets = eventd_sockets_get_all(bind_port, &private_socket, &unix_socket, take_over_socket);

    retval = eventd_service(sockets, no_avahi);

    eventd_sockets_free_all(sockets, unix_socket, private_socket);

    return retval;
}
