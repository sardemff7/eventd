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
#include <stdio.h>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include <glib.h>

#include "eventd.h"
#include "eventd-service.h"

#define PID_FILE "%s/pid"

#define DEFAULT_BIND_PORT 7100
#define UNIX_SOCKET "%s/sock"

static gboolean daemonize = FALSE;
static gchar *pid_file = NULL;

static gboolean action_kill = FALSE;

static guint16 bind_port = DEFAULT_BIND_PORT;

#ifdef ENABLE_GIO_UNIX
static gboolean no_network = FALSE;
static gboolean no_unix = FALSE;
static gchar *unix_socket = NULL;
#endif /* ENABLE_GIO_UNIX */

static GOptionEntry entries[] =
{
	{ "daemonize", 'd', 0, G_OPTION_ARG_NONE, &daemonize, "Run the daemon in the background", NULL },
	{ "pid-file", 'P', 0, G_OPTION_ARG_FILENAME, &pid_file, "Path to the pid file", "filename" },
	{ "kill", 'k', 0, G_OPTION_ARG_NONE, &action_kill, "Kill the running daemon", NULL },
	{ "port", 'p', 0, G_OPTION_ARG_INT, &bind_port, "Port to listen for inbound connections", "P" },
#ifdef ENABLE_GIO_UNIX
	{ "no-network", 'N', 0, G_OPTION_ARG_NONE, &no_network, "Disable the network bind", NULL },
	{ "no-unix", 'U', 0, G_OPTION_ARG_NONE, &no_unix, "Disable the UNIX socket bind", NULL },
	{ "socket", 's', 0, G_OPTION_ARG_FILENAME, &unix_socket, "UNIX socket to listen for inbound connections", "SOCKET_FILE" },
#endif /* ENABLE_GIO_UNIX */
	{ NULL }
};

gchar const *home = NULL;
int
main(int argc, char *argv[])
{
	int retval = 0;
	GError *error = NULL;
	gchar const *xdg_runtime_dir = NULL;
	gchar *run_dir = NULL;
	GOptionContext *context = NULL;

	#ifdef ENABLE_NLS
		setlocale(LC_ALL, "");
		bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
		bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	#endif /* ENABLE_NLS */

	g_type_init();

	home = g_getenv("HOME");
	xdg_runtime_dir = g_get_user_runtime_dir();

	run_dir = g_strdup_printf("%s/%s", xdg_runtime_dir, PACKAGE_NAME);
	if ( g_mkdir_with_parents(run_dir, 0755) < 0 )
		g_error("Can't create the run dir (%s): %s", run_dir, strerror(errno));


	context = g_option_context_new("- small daemon to act on remote or local events");
	g_option_context_add_main_entries(context, entries, GETTEXT_PACKAGE);
	if ( ! g_option_context_parse(context, &argc, &argv, &error) )
		g_error("Option parsing failed: %s\n", error->message);

#ifdef ENABLE_GIO_UNIX
	if ( ( no_network ) && ( no_unix ) )
		g_error("Nothing to bind to, kind of useless, isn't it?");

	if ( no_network )
		bind_port = 0;

	if ( no_unix )
	{
		g_free(unix_socket);
		unix_socket = NULL;
	}
	else if ( unix_socket == NULL )
		unix_socket = g_strdup_printf(UNIX_SOCKET, run_dir);
	else
	{
		gchar *t = unix_socket;
		if ( g_ascii_strncasecmp(t, "/", 1) != 0 )
			unix_socket = g_strdup_printf("%s/%s", home, t);
		else
			unix_socket = g_strdup(t);
		g_free(t);
	}
#endif /* ENABLE_GIO_UNIX */

	if ( pid_file == NULL )
		pid_file = g_strdup_printf(PID_FILE, run_dir);
	else
	{
		gchar *t = pid_file;
		if ( g_ascii_strncasecmp(t, "/", 1) != 0 )
			pid_file = g_strdup_printf("%s/%s", home, t);
		else
			pid_file = g_strdup(t);
		g_free(t);
	}

	g_free(run_dir);

	if ( action_kill )
	{
		gchar *contents = NULL;
		guint64 pid = 0;
		if ( ! g_file_get_contents(pid_file, &contents, NULL, &error) )
			g_warning("Unable to open pid file: %s", error->message);
		g_clear_error(&error);
		pid = g_ascii_strtoull(contents, NULL, 10);
		g_free(contents);
		kill(pid, SIGTERM);
		return 0;
	}

	#if DEBUG
	daemonize = FALSE;
	#endif /* ! DEBUG */
	if ( daemonize )
	{
		pid_t pid = -1;

		pid = fork();
		if ( pid == -1 )
		{
			perror("fork");
			exit(1);
		}
		else if ( pid != 0 )
		{
			FILE *f = NULL;

			f = g_fopen(pid_file, "w");
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
	}

	retval = eventd_service(bind_port, unix_socket);

	if ( daemonize )
		g_unlink(pid_file);
	g_free(pid_file);

	return retval;
}
