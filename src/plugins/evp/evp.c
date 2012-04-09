/*
 * eventd - Small daemon to act on remote or local events
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

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <libeventd-event.h>
#include <eventd-core-interface.h>
#include <eventd-plugin.h>
#include <libeventd-config.h>

#include "avahi.h"

#if ENABLE_AVAHI
#define AVAHI_OPTION_FLAG 0
#else /* ! ENABLE_AVAHI */
#define AVAHI_OPTION_FLAG G_OPTION_FLAG_HIDDEN
#endif /* ! ENABLE_AVAHI */

struct _EventdPluginContext {
    EventdCoreContext *core;
    EventdCoreInterface *core_interface;
    EventdEvpAvahiContext *avahi;
    gint64 max_clients;
    gboolean default_bind;
    gboolean default_unix;
    gchar **binds;
    gboolean no_avahi;
    gchar *avahi_name;
    GSocketService *service;
    GSList *clients;
    GHashTable *events;
};

typedef struct {
    GCancellable *cancellable;
    GDataInputStream *input;
    GDataOutputStream *output;
    gchar *category;
} EventdEvpClient;

static void
_eventd_service_client_disconnect(gpointer data)
{
    GCancellable *cancellable = data;
    g_cancellable_cancel(cancellable);
}

static gchar *
_eventd_evp_read_message(EventdEvpClient *client, GError **error)
{
    gsize size;
    gchar *line;
    line = g_data_input_stream_read_upto(client->input, "\n", -1, &size, client->cancellable, error);
    if ( line != NULL )
    {
        if ( g_data_input_stream_read_byte(client->input, client->cancellable, error) != '\n' )
            line = (g_free(line), NULL);
    }
    return line;
}

static gboolean
_eventd_evp_handshake(EventdEvpClient *client, GError **error)
{
    gchar *line;
    gboolean ret = FALSE;

    line =_eventd_evp_read_message(client, error);
    if ( line == NULL )
        return FALSE;

    if ( ! g_str_has_prefix(line, "HELLO ") )
        goto fail;

    if ( ! g_data_output_stream_put_string(client->output, "HELLO\n", client->cancellable, error) )
        goto fail;

    client->category = g_strdup(line+6);
    ret = TRUE;

fail:
    g_free(line);
    return ret;
}

static gchar *
_eventd_evp_event_data(EventdEvpClient *client, GError **error)
{
    gchar *data = NULL;

    gchar *line;

    while ( ( line =_eventd_evp_read_message(client, error) ) != NULL )
    {
        if ( g_strcmp0(line, ".") == 0 )
        {
            g_free(line);
            return data;
        }

        if ( data == NULL )
        {
            if ( line[0] == '.' )
                data = g_strdup(line+1);
            else
            {
                data = line;
                line = NULL;
            }
        }
        else
        {
            gchar *old = data;
            data = g_strconcat(old, "\n", ( line[0] == '.' ) ? line+1 : line, NULL);
            g_free(old);
        }
        g_free(line);
    }
    g_free(line);
    g_free(data);

    return NULL;
}

static gboolean
_eventd_evp_event(EventdEvpClient *client, EventdEvent *event, GError **error)
{
    gboolean ret = TRUE;

    gchar *line;

    while ( ( line =_eventd_evp_read_message(client, error) ) != NULL )
    {
        if ( g_strcmp0(line, ".") == 0 )
        {
            if ( ! g_data_output_stream_put_string(client->output, "OK\n", client->cancellable, error) )
                ret = FALSE;
            break;
        }

        if ( g_str_has_prefix(line, "DATA ") )
            eventd_event_add_data(event, g_strdup(line+5), _eventd_evp_event_data(client, error));
        else if ( g_str_has_prefix(line, "DATAL ") )
        {
            gchar **data = NULL;

            data = g_strsplit(line+6, " ", 2);
            eventd_event_add_data(event, data[0], data[1]);
            g_free(data);
        }
        else if ( g_str_has_prefix(line, "CATEGORY ") )
            eventd_event_set_category(event, line+9);
        else
            g_warning("[EVENT] Unknown message: %s", line);

        g_free(line);
    }
    g_free(line);

    if ( ! ret )
        g_object_unref(event);

    return ret;
}

static void
_eventd_evp_main(EventdPluginContext *context, EventdEvpClient *client, GError **error)
{
    gchar *line;

    while ( ( line =_eventd_evp_read_message(client, error) ) != NULL )
    {
        if ( g_strcmp0(line, "BYE") == 0 )
            break;

        if ( g_str_has_prefix(line, "EVENT ") )
        {
            EventdEvent *event;

            event = eventd_event_new(line+6);
            eventd_event_set_category(event, client->category);
            if ( ! _eventd_evp_event(client, event, error) )
                break;

            if ( ! GPOINTER_TO_UINT(libeventd_config_events_get_event(context->events, eventd_event_get_category(event), eventd_event_get_name(event))) )
                context->core_interface->push_event(context->core, event);
            g_object_unref(event);
        }
        else
            g_warning("Unknown message: %s", line);

        g_free(line);
    }
    g_free(line);
}

static gboolean
_eventd_service_connection_handler(GThreadedSocketService *socket_service, GSocketConnection *connection, GObject *source_object, gpointer user_data)
{
    EventdPluginContext *service = user_data;

    EventdEvpClient client = {0};
    GError *error = NULL;

    client.cancellable = g_cancellable_new();

    service->clients = g_slist_prepend(service->clients, client.cancellable);

    client.input = g_data_input_stream_new(g_io_stream_get_input_stream(G_IO_STREAM(connection)));
    client.output = g_data_output_stream_new(g_io_stream_get_output_stream(G_IO_STREAM(connection)));

    if ( _eventd_evp_handshake(&client, &error) )
        _eventd_evp_main(service, &client, &error);

    if ( ( error != NULL ) && ( error->code != G_IO_ERROR_CANCELLED ) )
        g_warning("Can't read the socket: %s", error->message);
    g_clear_error(&error);

    g_free(client.category);

    if ( ! g_io_stream_close(G_IO_STREAM(connection), NULL, &error) )
        g_warning("Can't close the stream: %s", error->message);
    g_clear_error(&error);

    service->clients = g_slist_remove(service->clients, client.cancellable);
    g_object_unref(client.cancellable);

    return TRUE;
}

static EventdPluginContext *
_eventd_evp_init(EventdCoreContext *core, EventdCoreInterface *core_interface)
{
    EventdPluginContext *service;

    service = g_new0(EventdPluginContext, 1);

    service->core = core;
    service->core_interface= core_interface;

    service->max_clients = -1;

    service->avahi_name = g_strdup(PACKAGE_NAME);

    service->events = libeventd_config_events_new(NULL);

    return service;
}

static void
_eventd_evp_uninit(EventdPluginContext *service)
{
    g_hash_table_unref(service->events);

    g_free(service->avahi_name);

    g_free(service);
}

static GList *
_eventd_evp_add_socket(GList *used_sockets, EventdPluginContext *context, const gchar * const *binds)
{
    GList *sockets = NULL;
    GList *socket_;

    sockets = context->core_interface->get_sockets(context->core, binds);

    for ( socket_ = sockets ; socket_ != NULL ; socket_ = g_list_next(socket_) )
    {
        GError *error = NULL;

        if ( ! g_socket_listener_add_socket(G_SOCKET_LISTENER(context->service), socket_->data, NULL, &error) )
        {
            g_warning("Unable to add socket: %s", error->message);
            g_clear_error(&error);
        }
        else
            used_sockets = g_list_prepend(used_sockets, g_object_ref(socket_->data));
    }
    g_list_free_full(sockets, g_object_unref);

    return used_sockets;
}

static void
_eventd_evp_start(EventdPluginContext *service)
{
    GList *sockets = NULL;

    service->service = g_threaded_socket_service_new(service->max_clients);

    if ( service->binds != NULL )
    {
        sockets = _eventd_evp_add_socket(sockets, service, (const gchar * const *)service->binds);
        g_strfreev(service->binds);
    }

    if ( service->default_bind )
    {
        const gchar *binds[] = { "tcp:" DEFAULT_BIND_PORT_STR, NULL };
        sockets = _eventd_evp_add_socket(sockets, service, binds);
    }

#if HAVE_GIO_UNIX
    if ( service->default_unix )
    {
        const gchar *binds[] = { "unix-runtime:" UNIX_SOCKET, NULL };
        sockets = _eventd_evp_add_socket(sockets, service, binds);
    }
#endif /* HAVE_GIO_UNIX */

    g_signal_connect(service->service, "run", G_CALLBACK(_eventd_service_connection_handler), service);

#if ENABLE_AVAHI
    if ( ! service->no_avahi )
        service->avahi = eventd_evp_avahi_start(service->avahi_name, sockets);
    else
#endif /* ENABLE_AVAHI */
        g_list_free_full(sockets, g_object_unref);
}

static void
_eventd_evp_stop(EventdPluginContext *service)
{
#if ENABLE_AVAHI
    eventd_evp_avahi_stop(service->avahi);
#endif /* ENABLE_AVAHI */

    g_slist_free_full(service->clients, _eventd_service_client_disconnect);

    g_socket_service_stop(service->service);
    g_socket_listener_close(G_SOCKET_LISTENER(service->service));
    g_object_unref(service->service);
}

static GOptionGroup *
_eventd_evp_get_option_group(EventdPluginContext *context)
{
    GOptionGroup *option_group;
    GOptionEntry entries[] =
    {
        { "listen-default",      'L', 0,                 G_OPTION_ARG_NONE,         &context->default_bind, "Listen on default interface",   NULL },
        { "listen",              'l', 0,                 G_OPTION_ARG_STRING_ARRAY, &context->binds,        "Add a socket to listen to",     "<socket>" },
#if HAVE_GIO_UNIX
        { "listen-default-unix", 'u', 0,                 G_OPTION_ARG_NONE,         &context->default_unix, "Listen on default UNIX socket", NULL },
#endif /* HAVE_GIO_UNIX */
        { "no-avahi",            'A', AVAHI_OPTION_FLAG, G_OPTION_ARG_NONE,         &context->no_avahi,     "Disable avahi publishing",      NULL },
        { NULL }
    };

    option_group = g_option_group_new("event", "EVENT protocol plugin options", "Show EVENT plugin help options", NULL, NULL);
    g_option_group_set_translation_domain(option_group, GETTEXT_PACKAGE);
    g_option_group_add_entries(option_group, entries);
    return option_group;
}

static void
_eventd_evp_config_reset(EventdPluginContext *context)
{
    g_hash_table_remove_all(context->events);
}

static void
_eventd_evp_global_parse(EventdPluginContext *context, GKeyFile *config_file)
{
    Int integer;
    gchar *avahi_name;

    if ( ! g_key_file_has_group(config_file, "server") )
        return;

    if ( libeventd_config_key_file_get_int(config_file, "server", "max-clients", &integer) < 0 )
        return;
    if ( integer.set )
        context->max_clients = integer.value;

    if ( libeventd_config_key_file_get_string(config_file, "server", "avahi-name", &avahi_name) < 0 )
        return;
    if ( avahi_name != NULL )
    {
        g_free(context->avahi_name);
        context->avahi_name = avahi_name;
    }
}

static void
_eventd_evp_event_parse(EventdPluginContext *context, const gchar *event_category, const gchar *event_name, GKeyFile *config_file)
{
    gboolean disable;
    gint8 r;
    gchar *name = NULL;
    gboolean parent = FALSE;

    if ( ! g_key_file_has_group(config_file, "event") )
        return;

    r = libeventd_config_key_file_get_boolean(config_file, "event", "disable", &disable);
    if ( r < 0 )
        return;

    name = libeventd_config_events_get_name(event_category, event_name);
    if ( event_name != NULL )
        parent = GPOINTER_TO_UINT(g_hash_table_lookup(context->events, event_category));

    if ( disable && ( ! parent ) )
        g_hash_table_insert(context->events, name, GUINT_TO_POINTER(disable));
    else if ( ( r == 0 ) && ( ! disable ) && parent )
        g_hash_table_insert(context->events, name, GUINT_TO_POINTER(disable));
}

const gchar *eventd_plugin_id = "eventd-evp";
void
eventd_plugin_get_info(EventdPlugin *plugin)
{
    plugin->init = _eventd_evp_init;
    plugin->uninit = _eventd_evp_uninit;

    plugin->get_option_group = _eventd_evp_get_option_group;

    plugin->start = _eventd_evp_start;
    plugin->stop = _eventd_evp_stop;

    plugin->config_reset = _eventd_evp_config_reset;

    plugin->global_parse = _eventd_evp_global_parse;
    plugin->event_parse = _eventd_evp_event_parse;
}
