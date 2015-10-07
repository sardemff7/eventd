/*
 * eventc - Basic CLI client for eventd
 *
 * Copyright © 2011-2015 Quentin "Sardem FF7" Glidic
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <config.h>

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <libeventd-event.h>

#include <libeventc.h>


static EventcConnection *client = NULL;
static EventdEvent *event = NULL;
static GMainLoop *loop = NULL;

static gint tries = 0;
static gint max_tries = 3;
static gboolean wait_event_end = FALSE;

static void _eventc_send_event(void);
static gboolean _eventc_disconnect(gpointer user_data);

static gboolean
_eventc_connect(gpointer user_data)
{
    GError *error = NULL;
    if ( ! eventc_connection_connect_sync(client, &error) )
    {
        g_warning("Couldn't connect to host: %s", error->message);

        if ( ( max_tries > 0 ) && ( ++tries >= max_tries ) )
        {
            g_warning("Too many attempts, aborting");
            g_main_loop_quit(loop);
            return FALSE;
        }
        return TRUE;
    }

    if ( event != NULL )
        _eventc_send_event();

    return FALSE;
}

static void
_eventc_event_answer_callback(EventdEvent *event, const gchar *answer, gpointer user_data)
{
    g_print("%s\n", answer);

    GHashTable *answer_data;
    answer_data = eventd_event_get_all_answer_data(event);

    if ( answer_data == NULL )
        return;

    GHashTableIter iter;
    gchar *name, *data;
    g_hash_table_iter_init(&iter, answer_data);
    while ( g_hash_table_iter_next(&iter, (gpointer *) &name, (gpointer *) &data) )
        g_print("%s=%s\n", name, data);

    g_hash_table_unref(answer_data);
}

static void
_eventc_event_end_callback(EventdEvent *event, EventdEventEndReason reason, gpointer user_data)
{
    g_idle_add(_eventc_disconnect, NULL);
}

static void
_eventc_event_callback(EventcConnection *connection, EventdEvent *event, gpointer user_data)
{
    if ( wait_event_end )
    {
        g_signal_connect(event, "answered", G_CALLBACK(_eventc_event_answer_callback), NULL);
        g_signal_connect(event, "ended", G_CALLBACK(_eventc_event_end_callback), NULL);
    }
    else
        g_idle_add(_eventc_disconnect, NULL);
}

static void
_eventc_send_event(void)
{
    if ( wait_event_end )
    {
        g_signal_connect(event, "answered", G_CALLBACK(_eventc_event_answer_callback), NULL);
        g_signal_connect(event, "ended", G_CALLBACK(_eventc_event_end_callback), NULL);
    }

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
    const gchar *category = NULL;
    const gchar *name = NULL;
    guint n_length = 0, c_length = 0;
    gchar **event_data_name = NULL;
    gchar **event_data_content = NULL;
    gchar **answers = NULL;
    gboolean subscribe = FALSE;

    gboolean print_version = FALSE;

    GOptionEntry entries[] =
    {
        { "data-name",    'd', 0, G_OPTION_ARG_STRING_ARRAY, &event_data_name,    "Event data name to send",                                  "<name>" },
        { "data-content", 'c', 0, G_OPTION_ARG_STRING_ARRAY, &event_data_content, "Event data content to send (must be after a data-name)",   "<content>" },
        { "answer",       'a', 0, G_OPTION_ARG_STRING_ARRAY, &answers,            "Possibles answers to event",                               "<answer>" },
        { "host",         'h', 0, G_OPTION_ARG_STRING,       &host,               "Host to connect to (defaults to $EVENTC_HOST if defined)", "<host>" },
        { "max-tries",    'm', 0, G_OPTION_ARG_INT,          &max_tries,          "Maximum connection attempts (0 for infinite)",             "<times>" },
        { "wait",         'w', 0, G_OPTION_ARG_NONE,         &wait_event_end,     "Wait the end of the event",                                NULL },
        { "subscribe",    's', 0, G_OPTION_ARG_NONE,         &subscribe,          "Subscribe mode",                                           NULL },
        { "version",      'V', 0, G_OPTION_ARG_NONE,         &print_version,      "Print version",                                            NULL },
        { NULL }
    };
    GOptionContext *opt_context = g_option_context_new("- Basic CLI client for eventd");

    g_option_context_set_summary(opt_context, ""
        "Normal mode: eventc <event category> <event name>"
        "\n  eventc will connect to <host> and send an event."
        "\n  If --wait is specified, eventc will only return after receiving the ENDED message for the event."
        "\n\n"
        "Subscribe mode: eventc --subscribe [<event category>...]"
        "\n  eventc will connect to <host> and wait for an event of the specified categories. If no category is specified, it will wait for any event."
        "\n  If --wait is specified, eventc will only return after receiving the ENDED message for the event."
        "");

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

    if ( subscribe )
        goto post_args;

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

    n_length = ( event_data_name == NULL ) ? 0 : g_strv_length(event_data_name);
    c_length = ( event_data_content == NULL ) ? 0 : g_strv_length(event_data_content);
    if ( n_length != c_length )
    {
        g_warning("Not the same number of data names and data contents");
        g_strfreev(event_data_name);
        g_strfreev(event_data_content);
        goto end;
    }

    category = argv[1];
    name = argv[2];

post_args:
    r = 2; /* Arguments are fine, checking host */

    client = eventc_connection_new(host, &error);
    if ( client == NULL )
    {
        g_warning("Could not resolve '%s': %s", host, error->message);
        g_clear_error(&error);
        goto end;
    }

    r = 0; /* Host is fine */

    if ( subscribe )
    {
        eventc_connection_set_subscribe(client, TRUE);
        eventc_connection_set_subscriptions(client, ( argc < 2 ) ? NULL : g_strdupv(argv + 1));

        g_signal_connect(client, "event", G_CALLBACK(_eventc_event_callback), NULL);
        goto post_event;
    }

    eventc_connection_set_passive(client, !wait_event_end);

    event = eventd_event_new(category, name);

    guint i;
    for ( i = 0 ; i < n_length ; ++i )
        eventd_event_add_data(event, event_data_name[i], event_data_content[i]);
    g_free(event_data_name);
    g_free(event_data_content);

    if ( answers != NULL )
    {
        gchar **answer;
        for ( answer = answers ; *answer != NULL ; ++answer )
            eventd_event_add_answer(event, *answer);
        g_strfreev(answers);
    }

post_event:
    g_idle_add(_eventc_connect, NULL);

    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    if ( event != NULL )
        g_object_unref(event);

    g_object_unref(client);

end:
    g_free(host);

    return r;
}
