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
#define PID_FILE   ".local/var/eventd.pid"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/file.h>
#include <signal.h>
#include <errno.h>

#include "eventd.h"
#include "eventd-service.h"

static guint16 bind_port = DEFAULT_BIND_PORT;
static gboolean action_kill = FALSE;
static gchar *pid_file = NULL;
static GOptionEntry entries[] =
{
  { "port", 'p', 0, G_OPTION_ARG_INT, &bind_port, "Port to listen for inbound connections", "P" },
  { "kill", 'k', 0, G_OPTION_ARG_NONE, &action_kill, "Kill the running daemon", NULL },
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
	#endif
	g_type_init();
	GError *error = NULL;

	GOptionContext *context = g_option_context_new("- small daemon to act on remote or local events");
	g_option_context_add_main_entries(context, entries, GETTEXT_PACKAGE);
	if ( ! g_option_context_parse(context, &argc, &argv, &error) )
		g_error("Option parsing failed: %s\n", error->message);

	home = g_getenv("HOME");

	gchar *real_pid_file = NULL;
	if ( pid_file )
		real_pid_file = g_strdup(pid_file);
	else
		real_pid_file = g_strdup_printf("%s/%s", home, PID_FILE);

	if ( action_kill )
	{
		gchar *contents = NULL;
		if ( ! g_file_get_contents(real_pid_file, &contents, NULL, &error) )
			g_warning("Unable to open pid file: %s", error->message);
		g_clear_error(&error);
		guint64 pid = g_ascii_strtoull(contents, NULL, 10);
		g_free(contents);
		kill(pid, SIGTERM);
		return 0;
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
		FILE *f = g_fopen(real_pid_file, "w");
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

	eventd_service(bind_port);

	#if ! DEBUG
	g_unlink(real_pid_file);
	g_free(real_pid_file);
	#endif

	return 0;
}
