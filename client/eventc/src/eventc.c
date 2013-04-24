/*
 * eventc - Basic CLI client for eventd
 *
 * Copyright © 2011-2012 Quentin "Sardem FF7" Glidic
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <libeventd-event.h>
#include <libeventd-event-private.h>

#include <libeventc.h>


static EventcConnection *client;
static EventdEvent *event;
static GMainLoop *loop;

static gint tries = 0;
static gint max_tries = 3;
static gboolean wait_event_end = FALSE;

static void _eventc_connect(void);
static void _eventc_send_event(void);
static gboolean _eventc_disconnect(gpointer user_data);

static void
_eventc_connect_callback(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    eventc_connection_connect_finish(client, res, &error);
    if ( error != NULL )
    {
        g_warning("Couldn't connect to host: %s", error->message);

        if ( ( max_tries > 0 ) && ( ++tries >= max_tries ) )
        {
            g_warning("Too many attempts, aborting");
            g_main_loop_quit(loop);
        }
        else
            _eventc_connect();
        return;
    }
    _eventc_send_event();
}

static void
_eventc_connect(void)
{
    eventc_connection_connect(client, _eventc_connect_callback, NULL);
}

static void
_eventc_event_end_callback(EventdEvent *event, EventdEventEndReason reason, gpointer user_data)
{
    g_idle_add(_eventc_disconnect, NULL);
}

static void
_eventc_send_event(void)
{
    if ( wait_event_end )
        g_signal_connect(event, "ended", G_CALLBACK(_eventc_event_end_callback), NULL);

    GError *error = NULL;
    if ( ! eventc_connection_event(client, event, &error) )
        g_warning("Couldn't send event '%s', '%s': %s", eventd_event_get_category(event), eventd_event_get_name(event), error->message);
    if ( ! wait_event_end )
       _eventc_disconnect(NULL);
}

static gboolean
_eventc_disconnect(gpointer user_data)
{
    GError *error = NULL;
    if ( ! eventc_connection_close(client, &error) )
        g_warning("Couldn't disconnect from event: %s", error->message);
    g_main_loop_quit(loop);

    return FALSE;
}


int
main(int argc, char *argv[])
{
    int r = 0;
    gchar *host = NULL;
    gchar **event_data_name;
    gchar **event_data_content;

    gboolean print_version = FALSE;

    GOptionEntry entries[] =
    {
        { "data-name",    'd', 0, G_OPTION_ARG_STRING_ARRAY, &event_data_name,    "Event data name to send",                                "<name>" },
        { "data-content", 'c', 0, G_OPTION_ARG_STRING_ARRAY, &event_data_content, "Event data content to send (must be after a data-name)", "<content>" },
        { "host",         'h', 0, G_OPTION_ARG_STRING,       &host,               "Host to connect to",                                     "<host>" },
        { "max-tries",    'm', 0, G_OPTION_ARG_INT,          &max_tries,          "Maximum connection attempts (0 for infinite)",           "<times>" },
        { "wait",         'w', 0, G_OPTION_ARG_NONE,         &wait_event_end,     "Wait the end of the event",                              NULL },
        { "version",      'V', 0, G_OPTION_ARG_NONE,         &print_version,      "Print version",                                          NULL },
        { NULL }
    };
    GOptionContext *opt_context = g_option_context_new("<event category> <event name> - Basic CLI client for eventd");

    g_option_context_add_main_entries(opt_context, entries, GETTEXT_PACKAGE);

    GError *error = NULL;
    if ( ! g_option_context_parse(opt_context, &argc, &argv, &error) )
    {
        g_warning("Couldn't parse the arguments: %s", error->message);
        g_option_context_free(opt_context);
        return 1;
    }
    g_option_context_free(opt_context);

    if ( print_version )
    {
        g_print("eventc %s (using libeventc %s)\n",
            PACKAGE_VERSION,
            eventc_get_version());
        goto end;
    }

    r = 1; /* We are checking arguments */

    if ( argc < 2 )
    {
        g_print("You must define the category of the event.\n");
        goto end;
    }
    if ( argc < 3 )
    {
        g_print("You must define the name of the event.\n");
        goto end;
    }
    guint n_length, c_length;

    n_length = ( event_data_name == NULL ) ? 0 : g_strv_length(event_data_name);
    c_length = ( event_data_content == NULL ) ? 0 : g_strv_length(event_data_content);
    if ( n_length != c_length )
    {
        g_warning("Not the same number of data names and data contents");
        g_strfreev(event_data_name);
        g_strfreev(event_data_content);
        goto end;
    }

    r = 0; /* Arguments are fine */

    const gchar *category = argv[1];
    const gchar *name = argv[2];

    client = eventc_connection_new(host);
    eventc_connection_set_passive(client, !wait_event_end);

    event = eventd_event_new(category, name);

    GHashTable *data;

    data = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    guint i;
    for ( i = 0 ; i < n_length ; ++i )
        g_hash_table_insert(data, event_data_name[i], event_data_content[i]);
    g_free(event_data_name);
    g_free(event_data_content);

    eventd_event_set_all_data(event, data);

    _eventc_connect();

    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    g_object_unref(event);

    g_object_unref(client);

end:
    g_free(host);

    return r;
}
