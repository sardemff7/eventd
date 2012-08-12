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
    GHashTable *events;
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
#if DEBUG
            g_debug("Received line: %s", line);
#endif /* DEBUG */

        if ( g_data_input_stream_read_byte(client->input, client->cancellable, error) != '\n' )
            line = (g_free(line), NULL);
    }
    return line;
}

static gboolean
_eventd_evp_handshake(EventdEvpClient *client, GError **error)
{
    gchar *line;

    while ( ( line =_eventd_evp_read_message(client, error) ) != NULL )
    {
        if ( g_str_has_prefix(line, "HELLO ") )
        {
            client->category = g_strdup(line+6);
            g_free(line);

            if ( ! g_data_output_stream_put_string(client->output, "HELLO\n", client->cancellable, error) )
                break;

#if DEBUG
            g_debug("Successful handshake, client category: %s", client->category);
#endif /* DEBUG */

            return TRUE;
        }

        g_free(line);

        if ( ! g_data_output_stream_put_string(client->output, "ERROR bad-handshake\n", client->cancellable, error) )
            break;
    }

    return FALSE;
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
            g_free(line);
            return ret;
        }

        if ( g_str_has_prefix(line, "DATA ") )
        {
            gchar *data;

            data = _eventd_evp_event_data(client, error);
            if ( data != NULL )
                eventd_event_add_data(event, g_strdup(line+5), data);
            else
                ret = FALSE;
        }
        else if ( g_str_has_prefix(line, "DATAL ") )
        {
            gchar **data = NULL;

            data = g_strsplit(line+6, " ", 2);
            eventd_event_add_data(event, data[0], data[1]);
            g_free(data);
        }
        else if ( g_str_has_prefix(line, "ANSWER ") )
            eventd_event_add_answer(event, line+7);
        else if ( g_str_has_prefix(line, "CATEGORY ") )
            eventd_event_set_category(event, line+9);
        else
            ret = FALSE;

        g_free(line);
    }

    return FALSE;
}

static void
_event_evp_event_answered(EventdEvent *event, const gchar *answer, gpointer user_data)
{
    EventdEvpClient *client = user_data;

    const gchar *id;
    id = eventd_event_get_id(event);

    GHashTable *answer_data;
    answer_data = eventd_event_get_all_answer_data(event);

    gchar **messagev;
    messagev = g_new(gchar *, g_hash_table_size(answer_data) + 3);

    GHashTableIter iter;
    gchar *name;
    gchar *data;

    gsize i = 1;

    messagev[0] = g_strdup_printf("ANSWERED %s %s", id, answer);

    g_hash_table_iter_init(&iter, answer_data);
    while ( g_hash_table_iter_next(&iter, (gpointer *)&name, (gpointer *)&data) )
    {
        if ( g_utf8_strchr(data, -1, '\n') == NULL )
            messagev[i++] = g_strdup_printf("DATAL %s %s", name, data);
        else
        {
            gchar **datav;
            gchar **data_;

            datav = g_strsplit(data, "\n", -1);
            for ( data_ = datav ; *data_ != NULL ; ++data_ )
            {
                if ( (*data_)[0] == '.' )
                {
                    gchar *tmp = *data_;
                    *data_ = g_strconcat(".", tmp, NULL);
                    g_free(tmp);
                }
            }

            data = g_strjoinv("\n", datav);
            g_strfreev(datav);

            messagev[i++] = g_strdup_printf("DATA %s\n%s\n.", name, data);
        }
    }

    messagev[i++] = g_strdup(".\n");
    messagev[i] = NULL;

    gchar *message;
    message = g_strjoinv("\n", messagev);
    g_strfreev(messagev);

    GError *error = NULL;
    if ( ! g_data_output_stream_put_string(client->output, message, client->cancellable, &error) )
    {
        g_warning("Couldn't send ANSWERED message: %s", error->message);
        g_clear_error(&error);
    }
    g_free(message);
}

static void
_event_evp_event_ended(EventdEvent *event, EventdEventEndReason reason, gpointer user_data)
{
    EventdEvpClient *client = user_data;

    const gchar *id;
    id = eventd_event_get_id(event);

    const gchar *reason_text = "";
    switch ( reason )
    {
    case EVENTD_EVENT_END_REASON_NONE:
        reason_text = "none";
    break;
    case EVENTD_EVENT_END_REASON_TIMEOUT:
        reason_text = "timeout";
    break;
    case EVENTD_EVENT_END_REASON_USER_DISMISS:
        reason_text = "user-dismiss";
    break;
    case EVENTD_EVENT_END_REASON_CLIENT_DISMISS:
        reason_text = "client-dismiss";
    break;
    case EVENTD_EVENT_END_REASON_RESERVED:
        reason_text = "reserved";
    break;
    }

    gchar *line;
    line = g_strdup_printf("ENDED %s %s\n", id, reason_text);

    GError *error = NULL;
    if ( ! g_data_output_stream_put_string(client->output, line, client->cancellable, &error) )
    {
        g_warning("Couldn't send ENDED message: %s", error->message);
        g_clear_error(&error);
    }
    g_free(line);

    g_hash_table_remove(client->events, id);
}

static void
_eventd_evp_main(EventdPluginContext *context, EventdEvpClient *client, GError **error)
{
    gchar *line;
    guint id = 0;

    while ( ( line =_eventd_evp_read_message(client, error) ) != NULL )
    {
        if ( g_strcmp0(line, "BYE") == 0 )
            break;

        if ( g_str_has_prefix(line, "EVENT ") )
        {
            EventdEvent *event;

            event = eventd_event_new(line+6);
            eventd_event_set_category(event, client->category);
            if ( _eventd_evp_event(client, event, error) )
            {
                g_free(line);
                line = g_strdup_printf("EVENT %x\n", ++id);
                if ( ! g_data_output_stream_put_string(client->output, line, client->cancellable, error) )
                {
                    g_object_unref(event);
                    break;
                }

#if DEBUG
                g_debug("Received an event (category: %s): %s", eventd_event_get_category(event), eventd_event_get_name(event));
#endif /* DEBUG */
                const gchar *config_id;

                config_id = context->core_interface->get_event_config_id(context->core, event);
                g_debug("Event config id: %s", config_id);
                if ( config_id != NULL )
                {
                    gchar *tid;
                    tid = g_strdup_printf("%x", id);
                    g_hash_table_insert(client->events, tid, event);
                    eventd_event_set_id(event, tid);
                    eventd_event_set_config_id(event, config_id);
                    g_signal_connect(event, "answered", G_CALLBACK(_event_evp_event_answered), client);
                    g_signal_connect(event, "ended", G_CALLBACK(_event_evp_event_ended), client);
                    context->core_interface->push_event(context->core, event);
                }
                else
                    g_object_unref(event);
            }
            else
            {
#if DEBUG
                if ( *error == NULL )
                    g_debug("Received a malformed event (category: %s): %s", eventd_event_get_category(event), eventd_event_get_name(event));
#endif /* DEBUG */

                g_object_unref(event);

                if ( ( *error != NULL )
                     || ( ! g_data_output_stream_put_string(client->output, "ERROR bad-event\n", client->cancellable, error) ) )
                    break;
            }
        }
        else if ( g_str_has_prefix(line, "END ") )
        {
            EventdEvent *event;

            event = g_hash_table_lookup(client->events, line+4);
            if ( event != NULL )
            {
                g_free(line);
                line = g_strdup_printf("ENDING %s\n", eventd_event_get_id(event));
                if ( ! g_data_output_stream_put_string(client->output, line, client->cancellable, error) )
                    break;

#if DEBUG
                g_debug("Ending event '%s'", eventd_event_get_id(event));
#endif /* DEBUG */

                eventd_event_end(event, EVENTD_EVENT_END_REASON_CLIENT_DISMISS);
            }
            else
            {
#if DEBUG
                g_debug("Received an END message with a bad id '%s'", line+4);
#endif /* DEBUG */

                if ( ! g_data_output_stream_put_string(client->output, "ERROR bad-id\n", client->cancellable, error) )
                    break;
            }
        }
        else if ( ! g_data_output_stream_put_string(client->output, "ERROR unknown\n", client->cancellable, error) )
            break;
        else
        {
#if DEBUG
            g_debug("Unknown message: %s", line);
#endif /* DEBUG */
        }

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
    client.events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);

    service->clients = g_slist_prepend(service->clients, client.cancellable);

    client.input = g_data_input_stream_new(g_io_stream_get_input_stream(G_IO_STREAM(connection)));
    client.output = g_data_output_stream_new(g_io_stream_get_output_stream(G_IO_STREAM(connection)));

    if ( _eventd_evp_handshake(&client, &error) )
        _eventd_evp_main(service, &client, &error);

    if ( ( error != NULL ) && ( error->code != G_IO_ERROR_CANCELLED ) )
        g_warning("Can't read the socket: %s", error->message);
    g_clear_error(&error);

#if DEBUG
            g_debug("Client connection closde(category: %s)", client.category);
#endif /* DEBUG */

    GHashTableIter iter;
    gpointer tid;
    gpointer event;
    g_hash_table_iter_init(&iter, client.events);
    while ( g_hash_table_iter_next(&iter, &tid, &event) )
        g_signal_handlers_disconnect_by_data(event, &client);

    g_hash_table_unref(client.events);
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

    service->avahi_name = g_strdup_printf(PACKAGE_NAME " %s", g_get_host_name());

    service->events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

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

    if ( ! g_key_file_has_group(config_file, "Server") )
        return;

    if ( libeventd_config_key_file_get_int(config_file, "Server", "MaxClients", &integer) < 0 )
        return;
    if ( integer.set )
        context->max_clients = integer.value;

    if ( libeventd_config_key_file_get_string(config_file, "Server", "AvahiName", &avahi_name) < 0 )
        return;
    if ( avahi_name != NULL )
    {
        g_free(context->avahi_name);
        context->avahi_name = avahi_name;
    }
}

static void
_eventd_evp_event_parse(EventdPluginContext *context, const gchar *id, GKeyFile *config_file)
{
    gboolean disable;
    gint8 r;

    if ( ! g_key_file_has_group(config_file, "Event") )
        return;

    r = libeventd_config_key_file_get_boolean(config_file, "Event", "Disable", &disable);
    if ( r < 0 )
        return;

    if ( disable )
        g_hash_table_insert(context->events, g_strdup(id), GUINT_TO_POINTER(disable));
}

EVENTD_EXPORT const gchar *eventd_plugin_id = "eventd-evp";
EVENTD_EXPORT
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
