/*
 * eventc - Basic CLI client for eventd
 *
 * Copyright © 2011-2024 Morgane "Sardem FF7" Glidic
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

#include "config.h"

#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gio/gio.h>
#include "nkutils-uuid.h"
#include "nkutils-git-version.h"

#include "libeventd-event.h"
#include "libeventd-event-private.h"

#include "libeventc.h"

static EventcConnection *client = NULL;
static EventdEvent *event = NULL;
static GMainLoop *loop = NULL;

static gint tries = 0;
static gint max_tries = 3;

static void _eventc_send_event(void);
static gboolean _eventc_disconnect(gpointer user_data);

static void
_eventc_connect_callback(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    if ( ! eventc_connection_connect_finish(client, res, &error) )
    {
        g_warning("Couldn't connect to eventd: %s", error->message);

        if ( ( max_tries > 0 ) && ( ++tries >= max_tries ) )
        {
            g_warning("Too many attempts, aborting");
            g_main_loop_quit(loop);
            return;
        }
        eventc_connection_connect(client, _eventc_connect_callback, NULL);
        return;
    }

    if ( event != NULL )
        _eventc_send_event();
}

static void
_eventc_disconnected_callback(EventcConnection *connection, gpointer user_data)
{
    g_main_loop_quit(loop);
}

static void
_eventc_received_event_callback(EventcConnection *connection, EventdEvent *event, gpointer user_data)
{
    g_idle_add(_eventc_disconnect, NULL);
}

static void
_eventc_send_event(void)
{
    GError *error = NULL;
    if ( ! eventc_connection_send_event(client, event, &error) )
        g_warning("Couldn't send event '%s', '%s': %s", eventd_event_get_category(event), eventd_event_get_name(event), error->message);
    g_idle_add(_eventc_disconnect, NULL);
}

static gboolean
_eventc_disconnect(gpointer user_data)
{
    GError *error = NULL;
    if ( ! eventc_connection_close(client, &error) )
        g_warning("Couldn't disconnect from event: %s", error->message);

    return G_SOURCE_REMOVE;
}


int
main(int argc, char *argv[])
{
    int r = 0;
    NkUuid uuid = NK_UUID_INIT;
    gchar *uuid_string = NULL;
    gchar *uri = NULL;
    gchar *identity = NULL;
    GSocketConnectable *server_identity = NULL;
    gchar *certificate_file = NULL;
    gchar *key_file = NULL;
    GTlsCertificate *certificate = NULL;
    const gchar *category = NULL;
    const gchar *name = NULL;
    gchar **data_strv = NULL;
    gchar **data_string_strv = NULL;
    gchar **file_strv = NULL;
    GHashTable *data = NULL;
    gboolean subscribe = FALSE;
    gint ping_interval = 0;
    gboolean system_mode = FALSE;

    gboolean insecure = FALSE;
    gboolean print_version = FALSE;

    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, EVENTD_LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");

    /* No free on the data name since we re-use args strings directly */
    data = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, (GDestroyNotify) g_variant_unref);

    GOptionEntry entries[] =
    {
        { "data",          'd', 0, G_OPTION_ARG_STRING_ARRAY,   &data_strv,        "Event data to send",                                       "<name>=<content>" },
        { "data-string",   'D', 0, G_OPTION_ARG_STRING_ARRAY,   &data_string_strv, "Event data strings to send",                               "<name>=<string>" },
        { "data-file",     'f', 0, G_OPTION_ARG_FILENAME_ARRAY, &file_strv,        "Event data to send from a file",                           "<name>=[<mime-type>@]<filename>" },
        { "uri",           'u', 0, G_OPTION_ARG_STRING,         &uri,              "URI to connect to (defaults to $EVENTC_HOST if defined)",  "<URI>" },
        { "identity",      'i', 0, G_OPTION_ARG_STRING,         &identity,         "Server identity to check for in TLS certificate",          "<host>" },
        { "max-tries",     'm', 0, G_OPTION_ARG_INT,            &max_tries,        "Maximum connection attempts (0 for infinite)",             "<times>" },
        { "certificate",   'c', 0, G_OPTION_ARG_FILENAME,       &certificate_file, "TLS certicate file to use",                                "<certificate>" },
        { "key",           'k', 0, G_OPTION_ARG_FILENAME,       &key_file,         "TLS key file to use",                                      "<key>" },
        { "subscribe",     's', 0, G_OPTION_ARG_NONE,           &subscribe,        "Subscribe mode",                                           NULL },
        { "ping-interval", 'p', 0, G_OPTION_ARG_INT,            &ping_interval,    "Ping interval",                                            "<seconds>" },
#ifdef G_OS_UNIX
        { "system",        'S', 0, G_OPTION_ARG_NONE,           &system_mode,      "Talk to system eventd",                                    NULL },
#endif /* G_OS_UNIX */
        { "insecure",      0,   0, G_OPTION_ARG_NONE,           &insecure,         "Accept insecure certificates (unknown CA)",                NULL },
        { "version",       'V', 0, G_OPTION_ARG_NONE,           &print_version,    "Print version",                                            NULL },
        { .long_name = NULL }
    };
    GOptionContext *opt_context = g_option_context_new("- Basic CLI client for eventd");

    g_option_context_set_summary(opt_context, ""
        "Normal mode: eventc <event category> <event name> [<event UUID>]"
        "\n  eventc will connect to <URI> and send an event."
        "\n  You can update an event by passing its UUID as the third positional argument."
        "\n  The UUID may be pre-generated, or you can use the one eventc generated in a previous call."
        "\n  Passing '' (emtpy string) as UUID will make eventc print the generated UUID to stdout."
        "\n\n"
        "Subscribe mode: eventc --subscribe [<event category>...]"
        "\n  eventc will connect to <URI> and wait for an event of the specified categories. If no category is specified, it will wait for any event."
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
            NK_PACKAGE_VERSION,
            eventc_get_version());
        goto end;
    }

    r = 1; /* We are checking arguments */

    if ( subscribe )
        goto post_event_args;

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

    gchar *data_name;
    GVariant *data_content;
    if ( data_strv != NULL )
    {
        gchar **d;
        for ( d = data_strv ; *d != NULL ; ++d )
        {
            gchar *c;
            data_name = *d;

            c = g_utf8_strchr(data_name, -1, '=');
            if ( c == NULL )
            {
                g_print("Malformed data '%s': Data format is '<name>=<content>'\n", data_name);
                goto end;
            }
            *c++ = '\0';

            data_content = g_variant_parse(NULL, c, NULL, NULL, &error);
            if ( data_content == NULL )
            {
                g_print("Malformed data content '%s': %s\n", data_name, error->message);
                goto end;
            }

            g_hash_table_insert(data, data_name, g_variant_ref_sink(data_content));
        }
    }

    if ( data_string_strv != NULL )
    {
        gchar **d;
        for ( d = data_string_strv ; *d != NULL ; ++d )
        {
            gchar *c;
            data_name = *d;

            c = g_utf8_strchr(data_name, -1, '=');
            if ( c == NULL )
            {
                g_print("Malformed data '%s': Data format is '<name>=<content>'\n", data_name);
                goto end;
            }
            *c++ = '\0';

            data_content = g_variant_new_string(c);

            g_hash_table_insert(data, data_name, g_variant_ref_sink(data_content));
        }
    }

    if ( file_strv != NULL )
    {
        gchar **d;
        for ( d = file_strv ; *d != NULL ; ++d )
        {
            gchar *m, *f;
            data_name = *d;

            m = g_utf8_strchr(data_name, -1, '=');
            if ( m == NULL )
            {
                g_print("Malformed data '%s': Data format is '<name>=<mime-type>@<filename>'\n", data_name);
                goto end;
            }
            *m++ = '\0';

            f = g_utf8_strchr(m, -1, '@');
            if ( f != NULL )
                *f++ = '\0';
            else
                f = m;

            gchar *mime_type = NULL;
            if ( ( f - m ) < 2 )
            {
                gchar *type;
                gboolean uncertain;
                type = g_content_type_guess(f, NULL, 0, &uncertain);
                if ( ( type == NULL ) || uncertain || ( ( mime_type = g_content_type_get_mime_type(type) ) == NULL ) )
                {
                    g_print("Could not guess MIME type for file '%s'\n", f);
                    g_free(type);
                    goto end;
                }
                m = mime_type;
            }

            gchar *filedata;
            gsize length;
            if ( ! g_file_get_contents(f, &filedata, &length, &error) )
            {
                g_print("Could not get file contents '%s': %s\n", f, error->message);
                g_free(mime_type);
                goto end;
            }

            data_content = g_variant_new("(msmsv)", m, NULL, g_variant_new_from_data(G_VARIANT_TYPE_BYTESTRING, filedata, length, FALSE, g_free, filedata));
            g_free(mime_type);

            g_hash_table_insert(data, data_name, g_variant_ref_sink(data_content));
        }
    }

    category = argv[1];
    name = argv[2];

    if ( argc >= 4 )
    {
        uuid_string = argv[3];
        if ( ( *uuid_string != '\0' ) && ( ! nk_uuid_parse(&uuid, uuid_string) ) )
        {
            g_warning("Wrong UUID: %s", uuid_string);
            goto end;
        }
    }

post_event_args:
    if ( identity != NULL )
    {
        server_identity = g_network_address_new(identity, 0);
        if ( server_identity == NULL )
        {
            g_print("Server identity must be a well-formed server name or address\n");
            goto end;
        }
    }

    if ( certificate_file != NULL )
    {
        if ( key_file != NULL )
            certificate = g_tls_certificate_new_from_files(certificate_file, key_file, &error);
        else
            certificate = g_tls_certificate_new_from_file(certificate_file, &error);
        if ( certificate == NULL )
        {
            g_warning("Could not parse certificate file '%s': %s", certificate_file, error->message);
            g_clear_error(&error);
            goto end;
        }
    }

    r = 2; /* Arguments are fine, checking URI */

    if ( system_mode )
        g_setenv("XDG_RUNTIME_DIR", "/run", TRUE);

    client = eventc_connection_new(uri, &error);
    if ( client == NULL )
    {
        g_warning("Could not resolve '%s': %s", uri, error->message);
        g_clear_error(&error);
        goto end;
    }
    g_signal_connect(client, "disconnected", G_CALLBACK(_eventc_disconnected_callback), NULL);

    r = 0; /* URI is fine */

    eventc_connection_set_ping_interval(client, ping_interval);
    if ( server_identity != NULL )
        eventc_connection_set_server_identity(client, server_identity);
    if ( certificate != NULL )
        eventc_connection_set_certificate(client, certificate);
    eventc_connection_set_accept_unknown_ca(client, insecure);

    if ( subscribe )
    {
        eventc_connection_set_subscribe(client, TRUE);
        gint i;
        for ( i = 1 ; i < argc ; ++i )
            eventc_connection_add_subscription(client, g_strdup(argv[i]));

        g_signal_connect(client, "received-event", G_CALLBACK(_eventc_received_event_callback), NULL);
        goto post_event;
    }

    if ( uuid_string != NULL )
    {
        if ( *uuid_string != '\0' )
            event = eventd_event_new_for_uuid(uuid, category, name);
        else
        {
            event = eventd_event_new(category, name);
            g_print("%s\n", eventd_event_get_uuid(event));
        }
    }
    else
        event = eventd_event_new(category, name);

    GHashTableIter iter;
    g_hash_table_iter_init(&iter, data);
    while ( g_hash_table_iter_next(&iter, (gpointer *) &data_name, (gpointer *) &data_content) )
        eventd_event_add_data(event, g_strdup(data_name), data_content);

post_event:
    eventc_connection_connect(client, _eventc_connect_callback, NULL);

    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    eventd_event_unref(event);

    g_object_unref(client);

end:
    g_hash_table_unref(data);
    g_strfreev(file_strv);
    g_strfreev(data_string_strv);
    g_strfreev(data_strv);
    if ( certificate != NULL )
        g_object_unref(certificate);
    g_free(certificate_file);
    if ( server_identity != NULL )
        g_object_unref(server_identity);
    g_free(identity);
    g_free(uri);

    return r;
}
