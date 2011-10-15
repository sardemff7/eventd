/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011 Quentin "Sardem FF7" Glidic
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

#if HAVE_DIALOGS
#include "dialogs.h"
#endif /* HAVE_DIALOGS */

#include "events.h"


GHashTable *config = NULL;
GHashTable *clients_config = NULL;

typedef enum {
	ACTION_NONE = 0,
#if HAVE_DIALOGS
	ACTION_MESSAGE,
#endif /* HAVE_DIALOGS */
} EventdActionType;

typedef struct {
	EventdActionType type;
	void *data;
} EventdAction;

static EventdAction *
eventd_action_new(EventdActionType type, void *data)
{
	EventdAction *action = g_new0(EventdAction, 1);
	action->type = type;
	if ( data )
		action->data = data;
	return action;
}

static void
eventd_action_free(EventdAction *action)
{
	g_return_if_fail(action != NULL);
	switch ( action->type )
	{
		default:
			g_free(action->data);
		break;
	}
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
event_action(const gchar *client_type, const gchar *client_name, const gchar *action_type, const gchar *action_name, const gchar *action_data)
{
	GHashTable *type_config;
	GList *actions;

	#if ENABLE_SOUND
	eventd_pulse_event_action(client_type, client_name, action_type, action_name, action_data);
	#endif /* ENABLE_SOUND */

	#if ENABLE_NOTIFY
	eventd_notify_event_action(client_type, client_name, action_type, action_name, action_data);
	#endif /* ENABLE_NOTIFY */

	if ( clients_config == NULL )
		return;

	type_config = g_hash_table_lookup(clients_config, client_type);
	actions = g_hash_table_lookup(type_config, action_type);
	for ( ; actions ; actions = g_list_next(actions) )
	{
		EventdAction *action = actions->data;
		switch ( action->type )
		{
		#if HAVE_DIALOGS
		case ACTION_MESSAGE:
			create_dialog(action->data, client_name, action_data);
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

gint8
eventd_config_key_file_get_string(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *event, const gchar *type, gchar **value)
{
	GError *error = NULL;
	gint8 ret = 0;
	gchar *ret_value = NULL;

	ret_value = g_key_file_get_string(config_file, group, key, &error);
	if ( ( ! ret_value ) && ( error->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND ) )
	{
		ret = -1;
		g_warning("Can't set the %s action of event '%s' for client type '%s': %s", group, event, type, error->message);
	}
	else if ( ! ret_value )
	{
		ret = 1;
		*value = NULL;
	}
	else
		*value = ret_value;
	g_clear_error(&error);

	return ret;
}

static void
eventd_parse_client(gchar *type, gchar *config_dir_name)
{
	GError *error = NULL;
	GDir *config_dir = NULL;
	GHashTable *type_config = NULL;
	gchar *file = NULL;
	gchar *defaults_config_file_name = NULL;
	GKeyFile *defaults_config_file = NULL;

	defaults_config_file_name = g_strdup_printf("%s.conf", config_dir_name);
	if ( g_file_test(defaults_config_file_name, G_FILE_TEST_IS_REGULAR) )
	{
		defaults_config_file = g_key_file_new();
		if ( ! g_key_file_load_from_file(defaults_config_file, defaults_config_file_name, G_KEY_FILE_NONE, &error) )
		{
			g_warning("Can't read the defaults file '%s': %s", defaults_config_file_name, error->message);
			g_clear_error(&error);
			g_key_file_free(defaults_config_file);
			defaults_config_file = NULL;
		}
	}
	g_free(defaults_config_file_name);

	config_dir = g_dir_open(config_dir_name, 0, &error);
	if ( ! config_dir )
	{
		g_warning("Can't read the configuration directory '%s': %s", config_dir_name, error->message);
		g_clear_error(&error);
		return;
	}

	type_config = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)eventd_action_list_free);

	while ( ( file = (gchar *)g_dir_read_name(config_dir) ) != NULL )
	{
		gchar *event = NULL;
		gchar *config_file_name = NULL;
		GKeyFile *config_file = NULL;
		GList *list = NULL;

		if ( g_str_has_prefix(file, ".") || ( ! g_str_has_suffix(file, ".conf") ) )
			continue;

		event = g_strndup(file, strlen(file) - 5);

		#if DEBUG
		g_debug("Parsing event '%s' of client type '%s'", event, type);
		#endif /* DEBUG */

		config_file_name = g_strdup_printf("%s/%s", config_dir_name, file);
		config_file = g_key_file_new();
		if ( ! g_key_file_load_from_file(config_file, config_file_name, G_KEY_FILE_NONE, &error) )
			goto next;


		#if ENABLE_SOUND
		eventd_pulse_event_parse(type, event, config_file, defaults_config_file);
		#endif /* ENABLE_SOUND */

		#if ENABLE_NOTIFY
		eventd_notify_event_parse(type, event, config_file, defaults_config_file);
		#endif /* ENABLE_NOTIFY */

		#if HAVE_DIALOGS
		if ( g_key_file_has_group(config_file, "dialog") )
		{
			gchar *message = NULL;

			if ( eventd_config_key_file_get_string(config_file, "dialog", "message", event, type, &message) < 0 )
				goto skip_dialog;

			if ( ( ! message ) && ( defaults_config_file ) && g_key_file_has_group(defaults_config_file, "dialog") )
					eventd_config_key_file_get_string(defaults_config_file, "dialog", "message", "defaults", type, &message);

			if ( ! message )
				message = g_strdup("%s");

			list = g_list_prepend(list,
				eventd_action_new(ACTION_MESSAGE, message));

		skip_dialog:
			g_free(message);
		}
		#endif /* HAVE_DIALOGS */

		g_hash_table_insert(type_config, g_strdup(event), list);

	next:
		if ( error )
			g_warning("Can't read the configuration file '%s': %s", config_file_name, error->message);
		g_clear_error(&error);
		g_key_file_free(config_file);
		g_free(config_file_name);
	}

	g_hash_table_insert(clients_config, g_strdup(type), type_config);

	g_dir_close(config_dir);
	if ( defaults_config_file )
		g_key_file_free(defaults_config_file);
}

void
eventd_config_parser()
{
	GError *error = NULL;
	gchar *config_dir_name = NULL;
	GDir *config_dir = NULL;
	gchar *client_config_dir_name = NULL;
	gchar *file = NULL;
	gchar *config_file_name = NULL;
	GKeyFile *config_file = NULL;


	config_dir_name = g_strdup_printf("%s/"PACKAGE_NAME, g_get_user_config_dir());


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
		else
			g_signal_connect(monitor, "changed", eventd_config_parser, NULL);
		g_clear_error(&error);
		g_object_unref(dir);
	}

	config_file_name = g_strdup_printf("%s/"PACKAGE_NAME".conf", config_dir_name);
	config_file = g_key_file_new();
	if ( ! g_key_file_load_from_file(config_file, config_file_name, G_KEY_FILE_NONE, &error) )
		g_warning("Can't read the configuration file '%s': %s", config_file_name, error->message);
	else
		eventd_parse_server(config_file);
	g_clear_error(&error);
	g_key_file_free(config_file);
	g_free(config_file_name);

	#if ENABLE_SOUND
	eventd_pulse_config_init();
	#endif /* ENABLE_SOUND */

	#if ENABLE_NOTIFY
	eventd_notify_config_init();
	#endif /* ENABLE_NOTIFY */

	clients_config = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_hash_table_remove_all);
	config_dir = g_dir_open(config_dir_name, 0, &error);
	if ( ! config_dir )
		goto out;

	while ( ( file = (gchar *)g_dir_read_name(config_dir) ) != NULL )
	{
		if ( g_str_has_prefix(file, ".") )
			continue;

		client_config_dir_name = g_strdup_printf("%s/%s", config_dir_name, file);

		if ( g_file_test(client_config_dir_name, G_FILE_TEST_IS_DIR) )
		{
			if ( g_hash_table_lookup(config, file) == NULL )
			{
				GFile *dir = NULL;
				GFileMonitor *monitor = NULL;

				dir = g_file_new_for_path(client_config_dir_name);
				if ( ( monitor = g_file_monitor(dir, G_FILE_MONITOR_NONE, NULL, &error) ) == NULL )
					g_warning("Couldn't monitor the config directory '%s': %s", client_config_dir_name, error->message);
				else
					g_signal_connect(monitor, "changed", eventd_config_parser, NULL);
				g_clear_error(&error);
				g_object_unref(dir);
			}

			eventd_parse_client(file, client_config_dir_name);
		}

		g_free(client_config_dir_name);
	}
	g_dir_close(config_dir);

	if ( ! config )
		eventd_init_default_server_config();
out:
	if ( error )
		g_warning("Can't read the configuration directory: %s", error->message);
	g_clear_error(&error);
	g_free(config_dir_name);
}

void
eventd_config_clean()
{
	#if ENABLE_NOTIFY
	eventd_notify_config_clean();
	#endif /* ENABLE_NOTIFY */

	#if ENABLE_SOUND
	eventd_pulse_config_clean();
	#endif /* ENABLE_SOUND */

	g_hash_table_remove_all(config);
	g_hash_table_remove_all(clients_config);
}


guint64
eventd_config_get_guint64(const gchar *name)
{
	gchar *value = NULL;

	value = g_hash_table_lookup(config, name);

	return g_ascii_strtoull(value, NULL, 10);
}
