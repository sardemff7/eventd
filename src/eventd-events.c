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

#include "eventd-events.h"

#define CONFIG_DIR ".config/eventd"
#define SOUNDS_DIR ".local/share/sounds/eventd"

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


#if ENABLE_NOTIFY
static gboolean
notification_closed_cb(NotifyNotification *notification)
{
	return FALSE;
}
#endif /* ENABLE_NOTIFY */

GHashTable *config = NULL;

typedef enum {
	ACTION_SOUND = 0,
	ACTION_NOTIFY,
	ACTION_MESSAGE,
} EventdActionType;

typedef struct {
	EventdActionType type;
	gchar *data;
} EventdAction;

static EventdAction *
eventd_action_new(EventdActionType type, gchar *data)
{
	EventdAction *action = g_new0(EventdAction, 1);
	action->type = type;
	if ( data )
		action->data = g_strdup(data);
	return action;
}

static void
eventd_action_free(EventdAction *action)
{
	g_return_if_fail(action != NULL);
	//g_free(action->data);
	//g_free(action);
}

void
event_action(gchar *client_type, gchar *client_name, gchar *client_action, gchar *client_data)
{
	GError *error = NULL;
	GHashTable *type_config = g_hash_table_lookup(config, client_type);
	GList *actions = g_hash_table_lookup(type_config, client_action);
	for ( ; actions ; actions = g_list_next(actions) )
	{
		EventdAction *action = actions->data;
		gchar *msg;
		gchar *data = client_data;
		switch ( action->type )
		{
		#if ENABLE_SOUND
		case ACTION_SOUND:
			guint8 f = g_open(action->data, O_RDONLY);
			if ( ! f )
				g_warning("Can't open sound file");
			guint8 buf[BUFFER_SIZE];
			gssize r = 0;
			while ((r = read(f, buf, BUFFER_SIZE)) > 0)
			{
				if ( pa_simple_write(sound, buf, r, NULL) < 0)
					g_warning("Error while playing sound file");
			}
		break;
		#endif /* ENABLE_SOUND */
		#if ENABLE_NOTIFY
		case ACTION_NOTIFY:
			if ( data != NULL )
				data = g_markup_escape_text(data, -1);
			msg = g_strdup_printf(action->data, data ? data : "");
			g_free(data);
			NotifyNotification *notification = notify_notification_new(client_name, msg, NULL
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
			g_signal_connect(notification, "closed", G_CALLBACK(notification_closed_cb), NULL);
			g_object_unref(G_OBJECT(notification));
		break;
		#endif /* ENABLE_NOTIFY */
		#if HAVE_DIALOGS
		case ACTION_MESSAGE:
			#if ENABLE_GTK
			#error Not supported yet
			#else /* ! ENABLE_GTK */
			msg = g_strdup_printf(action->data, data ? data : "");
			do_it("zenity", "--info", "--title", client_name, "--text", msg, NULL);
			#endif /* ! ENABLE_GTK */
		break;
		#endif /* HAVE_DIALOGS */
		}
	}
}

void
eventd_config_parser()
{
	GError *error = NULL;

	if ( config )
		eventd_config_clean();
	config = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, (GDestroyNotify)g_hash_table_remove_all);

	gchar *config_dir_name = NULL;
	GDir *config_dir = NULL;

	config_dir_name = g_strdup_printf("%s/%s", home, CONFIG_DIR);
	config_dir = g_dir_open(config_dir_name, 0, &error);
	if ( ! config_dir )
		goto out;

	gchar *type = NULL;
	while ( ( type = (gchar *)g_dir_read_name(config_dir) ) != NULL )
	{
		gchar *config_file_name = g_strdup_printf("%s/%s", config_dir_name, type);
		GKeyFile *config_file = g_key_file_new();
		if ( ! g_key_file_load_from_file(config_file, config_file_name, G_KEY_FILE_NONE, &error) )
		{
			g_warning("Can't read the configuration file: %s", error->message);
			goto next;
		}

		GHashTable *type_config = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, (GDestroyNotify)eventd_action_free);

		gchar **groups = g_key_file_get_groups(config_file, NULL);
		gchar **group = NULL;
		for ( group = groups ; *group ; ++group )
		{
			gchar **keys = g_key_file_get_keys(config_file, *group, NULL, &error);
			if ( ! keys )
			{
				g_warning("Can't read the keys for group %s: %s", *group, error->message);
				continue;
			}

			GList *list = NULL;

			gchar **key = NULL;
			for ( key = keys ; *key ; ++key )
			{
				EventdAction *action = NULL;
				if ( 0 ) {}
				#if HAVE_SOUND
				else if ( g_ascii_strcasecmp(*key, "sound") == 0 )
				{
					gchar *filename = g_key_file_get_value(config_file, *group, *key, NULL);
					if ( filename[0] != '/')
						filename = g_strdup_printf("%s/%s/%s", home, SOUNDS_DIR, filename);
					else
						filename = g_strdup(filename);
					action = eventd_action_new(ACTION_SOUND, filename);
					g_free(filename);
				}
				#endif /* HAVE_SOUND */
				#if ENABLE_NOTIFY
				else if ( g_ascii_strcasecmp(*key, "notify") == 0 )
				{
					gchar *msg = g_key_file_get_value(config_file, *group, *key, NULL);
					action = eventd_action_new(ACTION_NOTIFY, msg);
				}
				#endif /* ENABLE_NOTIFY */
				#if HAVE_DIALOGS
				else if ( g_ascii_strcasecmp(*key, "dialog") == 0 )
				{
					gchar *msg = g_key_file_get_value(config_file, *group, *key, NULL);
					action = eventd_action_new(ACTION_MESSAGE, msg);
				}
				#endif /* HAVE_DIALOGS */
				else
				{
					g_warning("action %s not supported", *key);
					continue;
				}
				list = g_list_prepend(list, action);
			}
			g_strfreev(keys);

			g_hash_table_insert(type_config, g_strdup(*group), list);
		}
		g_strfreev(groups);

		g_hash_table_insert(config, g_strdup(type), type_config);
	next:
		g_key_file_free(config_file);
		g_free(config_file_name);
		//g_free(type);
	}
	if ( error )
		g_warning("Can't read the configuration directory: %s", error->message);
	g_clear_error(&error);

	g_dir_close(config_dir);
out:
	g_free(config_dir_name);
}

void
eventd_config_clean()
{
	g_hash_table_remove_all(config);
}
