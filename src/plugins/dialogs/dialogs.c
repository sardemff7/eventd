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

#include <eventd-plugin.h>
#include <plugin-helper.h>

static GHashTable *events = NULL;

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
    g_spawn_sync(g_getenv("HOME"), /* working_dir */
        argv,
        NULL, /* env */
        G_SPAWN_SEARCH_PATH, /* flags */
        NULL,   /* child setup */
        NULL,   /* user_data */
        NULL,   /* stdout */
        NULL,   /* sterr */
        &ret,   /* exit_status */
        &error);    /* error */
    g_clear_error(&error);
    return ( ret == 0);
}


static void
eventd_dialogs_start(gpointer user_data)
{
    eventd_plugin_helper_regex_init();
}

static void
eventd_dialogs_stop()
{
    eventd_plugin_helper_regex_clean();
}

static void
eventd_dialogs_event_parse(const gchar *type, const gchar *event, GKeyFile *config_file)
{
    gchar *message = NULL;
    gchar *name = NULL;

    if ( ! g_key_file_has_group(config_file, "dialog") )
        return;

    if ( eventd_plugin_helper_config_key_file_get_string(config_file, "dialog", "message", &message) < 0 )
        return;

    if ( ! message )
        message = g_strdup("$event-data[text]");

    if ( event != NULL )
        name = g_strconcat(type, "-", event, NULL);
    else
        name = g_strdup(type);

    g_hash_table_insert(events, name, message);
}

static void
eventd_dialogs_event_action(const gchar *client_type, const gchar *client_name, const gchar *event_type, GHashTable *event_data)
{
    gchar *name;
    gchar *message;
    gchar *msg;

    name = g_strconcat(client_type, "-", event_type, NULL);
    message = g_hash_table_lookup(events, name);
    g_free(name);
    if ( ( message == NULL ) && ( ( message = g_hash_table_lookup(events, client_type) ) == NULL ) )
        return;

    msg = eventd_plugin_helper_regex_replace_event_data(message, event_data, NULL);
    do_it("zenity", "--info", "--title", client_name, "--text", msg, NULL);
    g_free(msg);
}

static void
eventd_dialogs_config_init()
{
    events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
}

static void
eventd_dialogs_config_clean()
{
    g_hash_table_unref(events);
}

void
eventd_plugin_get_info(EventdPlugin *plugin)
{
    plugin->start = eventd_dialogs_start;
    plugin->stop = eventd_dialogs_stop;

    plugin->config_init = eventd_dialogs_config_init;
    plugin->config_clean = eventd_dialogs_config_clean;

    plugin->event_parse = eventd_dialogs_event_parse;
    plugin->event_action = eventd_dialogs_event_action;
}


