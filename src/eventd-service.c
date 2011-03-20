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
#include <pulse/pulseaudio.h>
#include <pulse/thread-mainloop.h>
#endif /* ENABLE_SOUND */

#include "eventd-events.h"
#include "eventd-service.h"

#define BUFFER_SIZE 1024
#define UNIX_SOCK VAR_RUN_DIR"/sock"

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
		else if ( g_ascii_strcasecmp(line, "BYE") == 0 )
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


static void
sig_reload_handler(int sig)
{
	eventd_config_parser();
}

#if ENABLE_SOUND
pa_threaded_mainloop *pa_loop = NULL;
pa_context *sound = NULL;

void
pa_context_state_callback(pa_context *c, void *userdata)
{
	pa_context_state_t state = pa_context_get_state(c);
	switch ( state )
	{
		case PA_CONTEXT_READY:
			pa_threaded_mainloop_signal(pa_loop, 0);
		default:
		break;
	}
}

void
pa_mainloop_wait_callback(pa_stream *s, int success, void *userdata)
{
	pa_threaded_mainloop_signal(pa_loop, 0);
}
#endif /* ENABLE_SOUND */

int
eventd_service(guint16 bind_port)
{
	int retval = 0;

	signal(SIGTERM, sig_quit_handler);
	signal(SIGINT, sig_quit_handler);
	signal(SIGUSR1, sig_reload_handler);

	GError *error = NULL;

	#if ENABLE_NOTIFY
	notify_init(PACKAGE_NAME);
	#endif /* ENABLE_NOTIFY */

	#if ENABLE_SOUND
	pa_loop = pa_threaded_mainloop_new();
	pa_threaded_mainloop_start(pa_loop);

	sound = pa_context_new(pa_threaded_mainloop_get_api(pa_loop), PACKAGE_NAME);
	if ( ! sound )
		g_error("Can't open sound system");
	pa_context_state_t state = pa_context_get_state(sound);
	pa_context_set_state_callback(sound, pa_context_state_callback, NULL);

	pa_threaded_mainloop_lock(pa_loop);
	pa_context_connect(sound, NULL, 0, NULL);
	pa_threaded_mainloop_wait(pa_loop);
	pa_threaded_mainloop_unlock(pa_loop);
	#endif /* ENABLE_SOUND */

	eventd_config_parser();

	GSocketService *service = g_threaded_socket_service_new(5);
	if ( ! g_socket_listener_add_inet_port((GSocketListener *)service, bind_port, NULL, &error) )
		g_warning("Unable to open socket: %s", error->message);
	/*
	 * TODO: Add a UNIX sock file
	 */
	g_clear_error(&error);

	g_signal_connect(G_OBJECT(service), "run", G_CALLBACK(connection_handler), NULL);

	loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(loop);

	#if ENABLE_SOUND
	pa_operation* op = pa_context_drain(sound, (pa_context_notify_cb_t)pa_mainloop_wait_callback, NULL);
	if ( op )
	{
		pa_threaded_mainloop_lock(pa_loop);
		pa_threaded_mainloop_wait(pa_loop);
		pa_operation_unref(op);
		pa_threaded_mainloop_unlock(pa_loop);
	}
	pa_context_disconnect(sound);
	pa_context_unref(sound);
	pa_threaded_mainloop_stop(pa_loop);
	pa_threaded_mainloop_free(pa_loop);
	#endif /* ENABLE_SOUND */

	g_main_loop_unref(loop);

	eventd_config_clean();

	#if ENABLE_NOTIFY
	notify_uninit();
	#endif /* ENABLE_NOTIFY */

	return retval;
}
