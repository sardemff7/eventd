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

#define CONFIG_DIR ".config/eventd"
#define SOUNDS_DIR ".local/share/sounds/eventd"

#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <signal.h>
#include <errno.h>

#include "eventd.h"
#include "eventd-events.h"

extern gchar const *home;

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

void
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
			gchar *es_data = NULL;
			if ( data != NULL )
				es_data = g_markup_escape_text(data, -1);
			msg = g_strdup_printf(msg, es_data ? es_data : "");
			g_free(es_data);
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
			#error Not supported yet
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
