/*
 * Eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011 Sardem FF7
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "eventd.h"

#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <signal.h>
#include <errno.h>

#include <glib.h>
#include <gio/gio.h>

#if ENABLE_SOUND
#include "eventd-pulse.h"
#endif /* ENABLE_SOUND */

#if ENABLE_NOTIFY
#include "eventd-notify.h"
#endif /* ENABLE_NOTIFY */

#include "eventd-events.h"
#include "eventd-service.h"

#define BUFFER_SIZE 1024

static gboolean
connection_handler(
	GThreadedSocketService *service,
	GSocketConnection      *connection,
	GObject                *source_object,
	gpointer                user_data)
{
	GDataInputStream *input = g_data_input_stream_new(g_io_stream_get_input_stream((GIOStream *)connection));

	GError *error = NULL;
	gsize size = 0;
	gchar *type = NULL;
	gchar *name = NULL;
	gchar *line = NULL;
	while ( ( line = g_data_input_stream_read_line(input, &size, NULL, &error) ) != NULL )
	{
		g_strchomp(line);
		if ( g_ascii_strncasecmp(line, "HELLO ", 6) == 0 )
		{
			gchar **hello = g_strsplit(line+6, " ", 2);
			type = g_strdup(g_strstrip(hello[0]));
			if ( hello[1] != NULL )
				name = g_strdup(g_strstrip(hello[1]));
			else
				name = g_strdup(type);
			g_strfreev(hello);
			#if DEBUG
			g_debug("Hello: type=%s name=%s", type, name);
			#endif /* DEBUG */
		}
		else if ( g_ascii_strncasecmp(line, "BYE", 3) == 0 )
		{
			#if DEBUG
			g_debug("Bye");
			#endif /* DEBUG */
			break;
		}
		else if ( g_ascii_strncasecmp(line, "EVENT ", 6) == 0 )
		{
			gchar **event = g_strsplit(line+6, " ", 2);
			gchar *action = g_strstrip(event[0]);
			gchar *data = NULL;
			if ( event[1] != NULL )
				data = g_strstrip(event[1]);
			#if DEBUG
			g_debug("Event: action=%s data=%s", action, data);
			#endif /* DEBUG */
			event_action(type, name, action, data);
			g_strfreev(event);
		}
		else
		{
			#if DEBUG
			g_debug("Input: %s", line);
			#endif /* DEBUG */
			g_warning("Unknown message");
		}
	}
	if ( error )
		g_warning("Can't read the socket: %s", error->message);
	g_clear_error(&error);

	g_free(type);
	g_free(name);

	if ( ! g_io_stream_close((GIOStream *)connection, NULL, &error) )
		g_warning("Can't close the stream: %s", error->message);
	g_clear_error(&error);

	return TRUE;
}

static GMainLoop *loop = NULL;

static void
sig_quit_handler(int sig)
{
	if ( loop )
		g_main_loop_quit(loop);
	else
		exit(1);
}


int
eventd_service(guint16 bind_port, const gchar* unix_socket_path)
{
	int retval = 0;

	signal(SIGTERM, sig_quit_handler);
	signal(SIGINT, sig_quit_handler);

	GError *error = NULL;

	#if ENABLE_NOTIFY
	eventd_notify_start();
	#endif /* ENABLE_NOTIFY */

	#if ENABLE_SOUND
	eventd_pulse_start();
	#endif /* ENABLE_SOUND */

	eventd_config_parser();

	GSocketService *service = g_threaded_socket_service_new(5);
	if ( bind_port != 0 )
	{
		if ( ! g_socket_listener_add_inet_port((GSocketListener *)service, bind_port, NULL, &error) )
			g_warning("Unable to open network socket: %s", error->message);
		g_clear_error(&error);
	}

	#ifdef ENABLE_GIO_UNIX
	if ( unix_socket_path != NULL )
	{
		GSocket *unix_socket = NULL;

		if ( ( unix_socket = g_socket_new(G_SOCKET_FAMILY_UNIX, G_SOCKET_TYPE_STREAM, 0, &error)  ) == NULL )
			g_warning("Unable to create a UNIX socket: %s", error->message);
		g_clear_error(&error);

		GSocketAddress *address = g_unix_socket_address_new(unix_socket_path);
		if ( ! g_socket_bind(unix_socket, address, TRUE, &error) )
			g_warning("Unable to bind the UNIX socket: %s", error->message);
		g_clear_error(&error);

		if ( ! g_socket_listen(unix_socket, &error) )
			g_warning("Unable to listen with the UNIX socket: %s", error->message);
		g_clear_error(&error);

		if ( ! g_socket_listener_add_socket((GSocketListener *)service, unix_socket, NULL, &error) )
			g_warning("Unable to open UNIX socket: %s", error->message);
		g_clear_error(&error);
	}
	#endif /* ENABLE_GIO_UNIX */

	g_signal_connect(G_OBJECT(service), "run", G_CALLBACK(connection_handler), NULL);

	loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(loop);
	g_main_loop_unref(loop);

	eventd_config_clean();

	#ifdef ENABLE_GIO_UNIX
	if ( unix_socket_path != NULL )
		g_unlink(unix_socket_path);
	#endif /* ENABLE_GIO_UNIX */

	#if ENABLE_SOUND
	eventd_pulse_stop();
	#endif /* ENABLE_SOUND */

	#if ENABLE_NOTIFY
	eventd_notify_stop();
	#endif /* ENABLE_NOTIFY */

	return retval;
}
