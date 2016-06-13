/*
 * eventc - Basic CLI client for eventd
 *
 * Copyright © 2011-2016 Quentin "Sardem FF7" Glidic
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

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif /* HAVE_LOCALE_H */

#include <glib.h>
#ifdef ENABLE_NLS
#include <glib/gi18n.h>
#endif /* ENABLE_NLS */
#include <glib-object.h>
#include <gio/gio.h>

#include <libeventd-event.h>

#include <libeventc.h>

typedef struct {
    gchar *name;
    GVariant *content;
} EventcData;

static EventcConnection *client = NULL;
static EventdEvent *event = NULL;
static GMainLoop *loop = NULL;

static gboolean use_websocket = FALSE;
static gint tries = 0;
static gint max_tries = 3;

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
_eventc_event_callback(EventcConnection *connection, EventdEvent *event, gpointer user_data)
{
    g_idle_add(_eventc_disconnect, NULL);
}

static void
_eventc_send_event(void)
{
    GError *error = NULL;
    if ( ! eventc_connection_event(client, event, &error) )
        g_warning("Couldn't send event '%s', '%s': %s", eventd_event_get_category(event), eventd_event_get_name(event), error->message);
    if ( use_websocket )
        g_idle_add(_eventc_disconnect, NULL);
    else
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
    gchar *identity = NULL;
    GSocketConnectable *server_identity = NULL;
    const gchar *category = NULL;
    const gchar *name = NULL;
    gchar **data_strv = NULL;
    EventcData *data = NULL;
    gboolean subscribe = FALSE;

    gboolean insecure = FALSE;
    gboolean print_version = FALSE;

    setlocale(LC_ALL, "");
#ifdef ENABLE_NLS
    bindtextdomain(GETTEXT_PACKAGE, EVENTD_LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
#endif /* ENABLE_NLS */

    GOptionEntry entries[] =
    {
        { "data",      'd', 0, G_OPTION_ARG_STRING_ARRAY, &data_strv,      "Event data to send",                                       "<name>=<content>" },
        { "host",      'h', 0, G_OPTION_ARG_STRING,       &host,           "Host to connect to (defaults to $EVENTC_HOST if defined)", "<host>" },
        { "identity",  'i', 0, G_OPTION_ARG_STRING,       &identity,       "Server identity to check for in TLS certificate",          "<host>" },
        { "max-tries", 'm', 0, G_OPTION_ARG_INT,          &max_tries,      "Maximum connection attempts (0 for infinite)",             "<times>" },
        { "subscribe", 's', 0, G_OPTION_ARG_NONE,         &subscribe,      "Subscribe mode",                                           NULL },
        { "websocket", 'w', 0, G_OPTION_ARG_NONE,         &use_websocket,  "Use WebSocket",                                            NULL },
        { "insecure",  0,   0, G_OPTION_ARG_NONE,         &insecure,       "Accept insecure certificates (unknown CA)",                NULL },
        { "version",   'V', 0, G_OPTION_ARG_NONE,         &print_version,  "Print version",                                            NULL },
        { NULL }
    };
    GOptionContext *opt_context = g_option_context_new("- Basic CLI client for eventd");

    g_option_context_set_summary(opt_context, ""
        "Normal mode: eventc <event category> <event name>"
        "\n  eventc will connect to <host> and send an event."
        "\n\n"
        "Subscribe mode: eventc --subscribe [<event category>...]"
        "\n  eventc will connect to <host> and wait for an event of the specified categories. If no category is specified, it will wait for any event."
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
            EVENTD_VERSION,
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

    if ( data_strv != NULL )
    {
        gsize l, i;
        gchar *c;
        l = g_strv_length(data_strv);
        data = g_new0(EventcData, l + 1);
        for ( i = 0 ; i < l ; ++i )
        {
            EventcData *d = data + i;

            c = g_utf8_strchr(data_strv[i], -1, '=');
            if ( c == NULL )
            {
                g_print("Malformed data '%s': Data format is '<name>=<content>'\n", data_strv[i]);
                goto end;
            }
            *c++ = '\0';

            d->content = g_variant_parse(NULL, c, NULL, NULL, &error);
            if ( d->content == NULL )
            {
                g_print("Malformed data content '%s': %s\n", data_strv[i], error->message);
                goto end;
            }
            d->name = g_strdup(data_strv[i]);
        }
        g_strfreev(data_strv);
        data_strv = NULL;
    }

    if ( identity != NULL )
    {
        server_identity = g_network_address_new(identity, 0);
        if ( server_identity == NULL )
        {
            g_print("Server identity must be a well-formed server name or address\n");
            goto end;
        }
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

    eventc_connection_set_use_websocket(client, use_websocket, NULL);
    if ( server_identity != NULL )
        eventc_connection_set_server_identity(client, server_identity);
    eventc_connection_set_accept_unknown_ca(client, insecure);

    if ( subscribe )
    {
        eventc_connection_set_subscribe(client, TRUE);
        gint i;
        for ( i = 1 ; i < argc ; ++i )
            eventc_connection_add_subscription(client, g_strdup(argv[i]));

        g_signal_connect(client, "event", G_CALLBACK(_eventc_event_callback), NULL);
        goto post_event;
    }

    event = eventd_event_new(category, name);

    if ( data != NULL )
    {
        gsize i;
        for ( i = 0 ; data[i].name != NULL ; ++i )
            eventd_event_add_data(event, data[i].name, data[i].content);
        g_free(data);
        data = NULL;
    }

post_event:
    g_idle_add(_eventc_connect, NULL);

    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    eventd_event_unref(event);

    g_object_unref(client);

end:
    if ( data != NULL )
    {
        gsize i;
        for ( i = 0 ; data[i].name != NULL ; ++i )
        {
            g_free(data[i].name);
            g_variant_unref(data[i].content);
        }
        g_free(data);
    }
    g_strfreev(data_strv);
    if ( server_identity != NULL )
        g_object_unref(server_identity);
    g_free(identity);
    g_free(host);

    return r;
}
