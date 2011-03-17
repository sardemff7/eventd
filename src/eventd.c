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

#define DEFAULT_BIND_PORT 7100
#define BUFFER_SIZE 1024
#define CONFIG_DIR ".config/eventd"
#define PID_FILE   ".local/var/eventd.pid"
#define SOUNDS_DIR ".local/share/sounds/eventd"

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <signal.h>
#include <errno.h>
#include <gio/gio.h>
#if ENABLE_NOTIFY
#include <libnotify/notify.h>
#endif
#if ENABLE_PULSE
#include <pulse/simple.h>
#endif

static const gchar *home = NULL;
#if ENABLE_PULSE
static pa_simple *sound = NULL;
static const pa_sample_spec sound_spec = {
	.format = PA_SAMPLE_S16LE,
	.rate = 44100,
	.channels = 2
};
#endif

#define MAX_ARGS 50
static int
do_it(gchar * path, gchar * arg, ...)
{
	GError *error = NULL;
	gint ret;
	gchar * argv[MAX_ARGS + 2];
	argv[0] = path;
	gsize argno = 0;
	va_list al;
	va_start(al, arg);
	while (arg && argno < MAX_ARGS)
	{
		argv[++argno] = arg;
		arg = va_arg(al, gchar *);
	}
	argv[++argno] = NULL;
	va_end(al);
	g_spawn_sync(home, /* working_dir */
		argv,
		NULL, /* env */
		G_SPAWN_SEARCH_PATH, /* flags */
		NULL,	/* child setup */
		NULL,	/* user_data */
		NULL,	/* stdout */
		NULL,	/* sterr */
		&ret,	/* exit_status */
		&error);	/* error */
	g_clear_error(&error);
	return ( ret == 0);
}

static void
event_action(gchar *type, gchar *name, gchar *action, gchar *data)
{
	GError *error = NULL;
	GKeyFile *config = g_key_file_new();
	gchar *config_file = g_strdup_printf("%s/%s/%s.conf", home, CONFIG_DIR, type);
	if ( ! g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, &error) )
	{
		g_warning("Can't read the configuration file: %s", error->message);
		goto out;
	}

	gchar **keys = g_key_file_get_keys(config, action, NULL, &error);
	if ( ! keys )
	{
		g_warning("Can't read the configuration: %s", error->message);
		goto out;
	}
	gchar **key = NULL;
	for ( key = keys; *key ; ++key )
	{
		gchar *argv[MAX_ARGS];
		if ( 0 ) {}
		#if HAVE_SOUND
		else if ( g_ascii_strcasecmp(*key, "sound") == 0 )
		{
			gchar *filename = g_key_file_get_value(config, action, *key, NULL);
			if ( filename[0] != '/')
				filename = g_strdup_printf("%s/%s/%s", home, SOUNDS_DIR, filename);
			else
				filename = g_strdup(filename);
			#if ENABLE_PULSE
			guint8 f = g_open(filename, O_RDONLY);
			if ( ! f )
				g_warning("Can't open sound file");
			guint8 buf[BUFFER_SIZE];
			gssize r = 0;
			g_printf("Playing song:");
			while ((r = read(f, buf, BUFFER_SIZE)) > 0)
			{
				if ( pa_simple_write(sound, buf, r, NULL) < 0)
					g_warning("Error while playing sound file");
			}
			g_printf(" played\n");
			#else
			do_it("paplay", filename, NULL);
			#endif
			g_free(filename);
		}
		#endif /* HAVE_SOUND */
		#if ENABLE_NOTIFY
		else if ( g_ascii_strcasecmp(*key, "notify") == 0 )
		{
			gchar *msg = g_key_file_get_value(config, action, *key, NULL);
			if ( data != NULL )
				msg = g_strdup_printf(msg, data);
			else
				msg = g_strdup(msg);
			NotifyNotification *notification = notify_notification_new(name, msg, NULL
			#if ! NOTIFY_CHECK_VERSION(0,7,0)
			, NULL
			#endif
			);
			notify_notification_set_urgency(notification, NOTIFY_URGENCY_NORMAL);
			notify_notification_set_timeout(notification, 1);
			if ( ! notify_notification_show(notification, &error) )
				g_warning("Can't show the notification: %s", error->message);
			g_clear_error(&error);
			g_free(msg);
			g_object_unref(G_OBJECT(notification));
		}
		#endif /* ENABLE_NOTIFY */
		#if HAVE_DIALOGS
		else if ( g_ascii_strcasecmp(*key, "dialog") == 0 )
		{
			gchar *msg = g_key_file_get_value(config, action, *key, NULL);
			if ( data != NULL )
				msg = g_strdup_printf(msg, data);
			else
				msg = g_strdup(msg);
			#if ENABLE_GTK
			#else
			do_it("zenity", "--info", "--title", name, "--text", msg, NULL);
			#endif
			g_free(msg);
		}
		#endif /* HAVE_DIALOGS */
	}
	g_strfreev(keys);
	g_key_file_free(config);
out:
	g_free(config_file);
}

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
		#if DEBUG
		g_debug("Input: %s", line);
		#endif
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
			#endif
		}
		else if ( g_ascii_strcasecmp(line, "BYE") == 0 )
			break;
		else if ( g_ascii_strncasecmp(line, "EVENT ", 6) == 0 )
		{
			gchar **event = g_strsplit(line+6, " ", 2);
			gchar *action = g_strstrip(event[0]);
			gchar *data = NULL;
			if ( event[1] != NULL )
				data = g_strstrip(event[1]);
			#if DEBUG
			g_debug("Event: action=%s data=%s", action, data);
			#endif
			event_action(type, name, action, data);
			g_strfreev(event);
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

void
sig_quit_handler(int sig)
{
	g_main_loop_quit(loop);
}

int
main(int argc, char *argv[])
{
	g_type_init();
	GError *error = NULL;
	home = g_getenv("HOME");
	gchar *pid_file = g_strdup_printf("%s/%s",
					  home,
					  PID_FILE);

	if ( argc > 1 )
	{
		if ( g_ascii_strcasecmp(argv[1], "--kill") == 0)
		{
			gchar *contents = NULL;
			if ( ! g_file_get_contents (pid_file, &contents, NULL, &error) )
				g_warning("Unable to open pid file: %s", error->message);
			g_clear_error(&error);
			guint64 pid = g_ascii_strtoull(contents, NULL, 10);
			g_free(contents);
			kill(pid, SIGTERM);
			return 0;
		}
	}
	#if ! DEBUG
	pid_t pid = fork();
	if ( pid == -1 )
	{
		perror("fork");
		exit(1);
	}
	else if ( pid != 0 )
	{
		FILE *f = g_fopen(pid_file, "w");
		g_fprintf(f, "%d", pid);
		g_free(f);
		return 0;
	}
	close(0);
	close(1);
	close(2);
	open("/dev/null", O_RDWR);
	dup2(0,1);
	dup2(0,2);
	#endif

	signal(SIGTERM, sig_quit_handler);
	signal(SIGINT, sig_quit_handler);

	#if ENABLE_NOTIFY
	notify_init("Eventd");
	#endif

	#if ENABLE_PULSE
	sound = pa_simple_new(NULL,         // Use the default server.
			"Eventd",        // Our application's name.
			PA_STREAM_PLAYBACK,
			NULL,               // Use the default device.
			"Sound events",     // Description of our stream.
			&sound_spec,        // Our sample format.
			NULL,               // Use default channel map
			NULL,               // Use default buffering attributes.
			NULL                // Ignore error code.
			);
	if ( ! sound )
		g_warning("Can't open sound system");
	#endif

	guint16 bind_port = DEFAULT_BIND_PORT;
	loop = g_main_loop_new(NULL, FALSE);

	GSocketService *service = g_threaded_socket_service_new(5);
	if ( ! g_socket_listener_add_inet_port((GSocketListener *)service, bind_port, NULL, &error) )
		g_warning("Unable to open socket: %s", error->message);
	g_clear_error(&error);

	g_signal_connect(G_OBJECT(service), "run", G_CALLBACK(connection_handler), NULL);

	g_main_loop_run(loop);

	g_main_loop_unref(loop);

	#if ENABLE_NOTIFY
	notify_uninit();
	#endif

	#if ENABLE_PULSE
	if ( pa_simple_drain(sound, NULL) < 0)
		g_warning("bug");
	pa_simple_free(sound);
	#endif

	#if ! DEBUG
	g_unlink(pid_file);
	g_free(pid_file);
	#endif

	return 0;
}
