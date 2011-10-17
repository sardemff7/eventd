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
eventd_dialogs_event_parse(const gchar *type, const gchar *event, GKeyFile *config_file, GKeyFile *defaults_config_file)
{
    gchar *message = NULL;
    gchar *name = NULL;

    if ( ! g_key_file_has_group(config_file, "dialog") )
        return;

    if ( eventd_config_key_file_get_string(config_file, "dialog", "message", event, type, &message) < 0 )
        goto skip;

    if ( ( ! message ) && ( defaults_config_file ) && g_key_file_has_group(defaults_config_file, "dialog") )
            eventd_config_key_file_get_string(defaults_config_file, "dialog", "message", "defaults", type, &message);

    if ( ! message )
        message = g_strdup("%s");

    name = g_strdup_printf("%s-%s", type, event);
    g_hash_table_insert(events, name, message);

skip:
    g_free(message);
}

static void
eventd_dialogs_event_action(const gchar *client_type, const gchar *client_name, const gchar *event_type, const gchar *event_name, const gchar *event_data)
{
    gchar *name;
    gchar *message;
    gchar *msg;

    name = g_strdup_printf("%s-%s", client_type, event_type);

    message = g_hash_table_lookup(events, name);
    if ( message == NULL )
        goto fail;

    msg = g_strdup_printf(message, event_data ? event_data : "");
    do_it("zenity", "--info", "--title", client_name, "--text", msg, NULL);
    g_free(msg);

fail:
    g_free(name);
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
    plugin->config_init = eventd_dialogs_config_init;
    plugin->config_clean = eventd_dialogs_config_clean;

    plugin->event_parse = eventd_dialogs_event_parse;
    plugin->event_action = eventd_dialogs_event_action;
}


