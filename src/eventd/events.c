/*
 * eventd - Small daemon to act on remote or local events
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

#define CONFIG_RELOAD_DELAY 10

#define DEFAULT_DELAY 5
#define DEFAULT_DELAY_STR "5"

#if ENABLE_SOUND
#include "pulse.h"
#endif /* ENABLE_SOUND */

#if ENABLE_NOTIFY
#include "notify.h"
#endif /* ENABLE_NOTIFY */

#include "events.h"

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


GHashTable *config = NULL;
GHashTable *clients_config = NULL;

typedef enum {
	ACTION_NONE = 0,
#if ENABLE_SOUND
	ACTION_SOUND ,
#endif /* ENABLE_SOUND */
#if ENABLE_NOTIFY
	ACTION_NOTIFY,
#endif /* ENABLE_NOTIFY */
#if HAVE_DIALOGS
	ACTION_MESSAGE,
#endif /* HAVE_DIALOGS */
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
	switch ( action->type )
	{
		#if ENABLE_SOUND
		case ACTION_SOUND:
			eventd_pulse_remove_sample(action->data);
		#endif /* ENABLE_SOUND */
		default:
		break;
	}
	g_free(action->data);
	g_free(action);
}

static void
eventd_action_list_free(GList *actions)
{
	g_return_if_fail(actions != NULL);
	GList *action;
	for ( action = g_list_first(actions) ; action ; action = g_list_next(action) )
		eventd_action_free(action->data);
}

void
event_action(gchar *client_type, gchar *client_name, gchar *client_action, gchar *client_data)
{
	GHashTable *type_config = g_hash_table_lookup(clients_config, client_type);
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
			eventd_pulse_play_sample(action->data);
		break;
		#endif /* ENABLE_SOUND */
		#if ENABLE_NOTIFY
		case ACTION_NOTIFY:
			if ( data != NULL )
				data = g_markup_escape_text(data, -1);
			msg = g_strdup_printf(action->data, data ? data : "");
			g_free(data);
			eventd_notify_display(client_name, msg);
			g_free(msg);
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
		default:
		break;
		}
	}
}

static void
eventd_parse_server(GKeyFile *config_file)
{
	GError *error = NULL;
	gchar **keys = NULL;
	gchar **key = NULL;

	config = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	if ( g_key_file_has_group(config_file, "server") )
	{
		keys = g_key_file_get_keys(config_file, "server", NULL, &error);
		for ( key = keys ; *key ; ++key )
			g_hash_table_insert(config, g_strdup(*key), g_key_file_get_value(config_file, "server", *key, NULL));
		g_strfreev(keys);
	}
}

static void
eventd_init_default_server_config()
{
	if ( ! config )
		config = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	if ( g_hash_table_lookup(config, "delay") == NULL )
		g_hash_table_insert(config, g_strdup("delay"), g_strdup(DEFAULT_DELAY_STR));
}

static void
eventd_parse_client(gchar *type, GKeyFile *config_file)
{
	GError *error = NULL;
	GHashTable *type_config = NULL;
	gchar **groups = NULL;
	gchar **group = NULL;

	type_config = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)eventd_action_list_free);
	groups = g_key_file_get_groups(config_file, NULL);

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
			#if ENABLE_SOUND
			else if ( g_ascii_strcasecmp(*key, "sound") == 0 )
			{
				gchar *filename = g_key_file_get_value(config_file, *group, *key, NULL);
				if ( filename[0] != '/')
					filename = g_strdup_printf("%s/"PACKAGE_NAME"/sounds/%s", g_get_user_data_dir(), filename);
				else
					filename = g_strdup(filename);
				gchar *sample_name = g_strdup_printf("%s-%s", type, *group);
				if ( eventd_pulse_create_sample(sample_name, filename) )
					action = eventd_action_new(ACTION_SOUND, sample_name);
				g_free(sample_name);
				g_free(filename);
			}
			#endif /* ENABLE_SOUND */
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
			if ( action )
				list = g_list_prepend(list, action);
		}
		g_strfreev(keys);

		g_hash_table_insert(type_config, g_strdup(*group), list);
	}
	g_strfreev(groups);

	g_hash_table_insert(clients_config, g_strdup(type), type_config);
}

void
eventd_config_parser()
{
	GError *error = NULL;
	gchar *config_dir_name = NULL;
	GDir *config_dir = NULL;
	gchar *file = NULL;

	#if ENABLE_SOUND
	gchar *sounds_dir_name = NULL;
	#endif /* ENABLE_SOUND */


	config_dir_name = g_strdup_printf("%s/"PACKAGE_NAME, g_get_user_config_dir());

	#if ENABLE_SOUND
	sounds_dir_name = g_strdup_printf("%s/"PACKAGE_NAME"/sounds", g_get_user_data_dir());
	#endif /* ENABLE_SOUND */


	if ( config )
	{
		g_message("Reloading configuration");
		eventd_config_clean();
	}
	else
	{
		/*
		 * We monitor the various dirs we use to
		 * reload the configuration if the user
		 * change it
		 */
		GFile *dir = NULL;
		GFileMonitor *monitor = NULL;

		g_message("First configuration load");

		dir = g_file_new_for_path(config_dir_name);
		if ( ( monitor = g_file_monitor(dir, G_FILE_MONITOR_NONE, NULL, &error) ) == NULL )
			g_warning("Couldn't monitor the main config directory: %s", error->message);
		g_signal_connect(monitor, "changed", eventd_config_parser, NULL);
		g_clear_error(&error);
		g_object_unref(dir);

		#if ENABLE_SOUND
		dir = g_file_new_for_path(sounds_dir_name);
		if ( ( monitor = g_file_monitor(dir, G_FILE_MONITOR_NONE, NULL, &error) ) == NULL )
			g_warning("Couldn't monitor the sounds directory: %s", error->message);
		g_signal_connect(monitor, "changed", eventd_config_parser, NULL);
		g_clear_error(&error);
		g_object_unref(dir);
		#endif /* ENABLE_SOUND */
	}

	clients_config = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_hash_table_remove_all);

	config_dir = g_dir_open(config_dir_name, 0, &error);
	if ( ! config_dir )
		goto out;

	while ( ( file = (gchar *)g_dir_read_name(config_dir) ) != NULL )
	{
		gchar *type = NULL;
		gchar *config_file_name = NULL;
		GKeyFile *config_file = NULL;

		if ( g_str_has_prefix(file, ".") || ( ! g_str_has_suffix(file, ".conf") ) )
			continue;

		type = g_strndup(file, strlen(file) - 5);
		config_file_name = g_strdup_printf("%s/%s", config_dir_name, file);
		config_file = g_key_file_new();
		if ( ! g_key_file_load_from_file(config_file, config_file_name, G_KEY_FILE_NONE, &error) )
		{
			g_warning("Can't read the configuration file: %s", error->message);
			goto next;
		}

		if ( g_ascii_strcasecmp(PACKAGE_NAME, type) == 0 )
			eventd_parse_server(config_file);
		else
			eventd_parse_client(type, config_file);
	next:
		g_key_file_free(config_file);
		g_free(config_file_name);
		g_free(type);
	}
	if ( error )
		g_warning("Can't read the configuration directory: %s", error->message);
	g_clear_error(&error);

	if ( ! config )
	{
		eventd_init_default_server_config();
	}

	g_dir_close(config_dir);
out:
	#if ENABLE_SOUND
	g_free(sounds_dir_name);
	#endif /* ENABLE_SOUND */
	g_free(config_dir_name);
}

void
eventd_config_clean()
{
	g_hash_table_remove_all(config);
	g_hash_table_remove_all(clients_config);
}


guint64
eventd_config_get_guint64(gchar *name)
{
	gchar *value = NULL;

	value = g_hash_table_lookup(config, name);

	return g_ascii_strtoull(value, NULL, 10);
}
