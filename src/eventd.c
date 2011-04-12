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

#define DEFAULT_BIND_PORT 7100

#define RUN_DIR "/run/%d/eventd"
#define UNIX_SOCKET RUN_DIR"/sock"
#define PID_FILE RUN_DIR"/pid"

static gboolean foreground = FALSE;

static gboolean action_kill = FALSE;
static gboolean action_reload = FALSE;

static gboolean no_network = FALSE;
static guint16 bind_port = DEFAULT_BIND_PORT;

static gboolean no_unix = FALSE;
static gchar *unix_socket = NULL;

static gchar *pid_file = NULL;

static GOptionEntry entries[] =
{
	{ "foreground", 'f', 0, G_OPTION_ARG_NONE, &foreground, "Run the daemon ine the foreground", NULL },
	{ "kill", 'k', 0, G_OPTION_ARG_NONE, &action_kill, "Kill the running daemon", NULL },
	{ "reload", 'r', 0, G_OPTION_ARG_NONE, &action_reload, "Reload the configuration", NULL },
#ifdef ENABLE_GIO_UNIX
	{ "no-network", 'N', 0, G_OPTION_ARG_NONE, &no_network, "Disable the network bind", NULL },
#endif /* ENABLE_GIO_UNIX */
	{ "port", 'p', 0, G_OPTION_ARG_INT, &bind_port, "Port to listen for inbound connections", "P" },
#ifdef ENABLE_GIO_UNIX
	{ "no-unix", 'U', 0, G_OPTION_ARG_NONE, &no_network, "Disable the UNIX socket bind", NULL },
	{ "socket", 'S', 0, G_OPTION_ARG_FILENAME, &unix_socket, "UNIX socket to listen for inbound connections", "S" },
#endif /* ENABLE_GIO_UNIX */
	{ "pid-file", 'P', 0, G_OPTION_ARG_FILENAME, &pid_file, "Path to the pid file", "filename" },
	{ NULL }
};

gchar const *home = NULL;
int
main(int argc, char *argv[])
{
	#ifdef ENABLE_NLS
		setlocale(LC_ALL, "");
		bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
		bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	#endif /* ENABLE_NLS */
	g_type_init();
	GError *error = NULL;

	home = g_getenv("HOME");
	uid_t uid = getuid();

	gchar *run_dir = g_strdup_printf(RUN_DIR, uid);
	if ( g_mkdir_with_parents(run_dir, 0755) < 0 )
		g_error("Can't create the run dir (%s): %s", run_dir, strerror(errno));
	g_free(run_dir);


	GOptionContext *context = g_option_context_new("- small daemon to act on remote or local events");
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
		unix_socket = g_strdup_printf(UNIX_SOCKET, uid);
	else
	{
		gchar *t = pid_file;
		if ( g_ascii_strncasecmp(t, "/", 1) != 0 )
			unix_socket = g_strdup_printf("%s/%s", home, t);
		else
			unix_socket = g_strdup(t);
		g_free(t);
	}
#endif /* ENABLE_GIO_UNIX */

	if ( pid_file == NULL )
		pid_file = g_strdup_printf(PID_FILE, uid);
	else
	{
		gchar *t = pid_file;
		if ( g_ascii_strncasecmp(t, "/", 1) != 0 )
			pid_file = g_strdup_printf("%s/%s", home, t);
		else
			pid_file = g_strdup(t);
		g_free(t);
	}

	if ( ( action_kill ) || ( action_reload ) )
	{
		gchar *contents = NULL;
		if ( ! g_file_get_contents(pid_file, &contents, NULL, &error) )
			g_warning("Unable to open pid file: %s", error->message);
		g_clear_error(&error);
		guint64 pid = g_ascii_strtoull(contents, NULL, 10);
		g_free(contents);
		kill(pid, ( action_kill ? SIGTERM : SIGUSR1 ));
		return 0;
	}

	#if DEBUG
	foreground = TRUE;
	#endif /* ! DEBUG */
	if ( ! foreground )
	{
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
	}

	int retval = eventd_service(bind_port, unix_socket);

	if ( ! foreground )
	{
		g_unlink(pid_file);
		g_free(pid_file);
	}

	return retval;
}
