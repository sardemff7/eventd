/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2016 Quentin "Sardem FF7" Glidic
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

#include <config.h>

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <libeventd-event.h>
#include <libeventd-protocol.h>
#include <eventd-plugin.h>
#include <libeventd-helpers-config.h>

#include "../eventd.h"
#include "evp-internal.h"

#include "client.h"

struct _EventdEvpClient {
    EventdEvpContext *context;
    GList *link;
    EventdProtocol *protocol;
    GCancellable *cancellable;
    GSocketConnection *connection;
    gboolean first;
    GIOStream *tls;
    EventdWsConnection *ws;
    GDataInputStream *in;
    GDataOutputStream *out;
    EventdEvent *current;
    GList *subscribe_all;
    GHashTable *subscriptions;
};


static void _eventd_evp_client_disconnect_internal(EventdEvpClient *self);

static void
_eventd_evp_client_send_message(EventdEvpClient *self, gchar *message)
{
    GError *error = NULL;

#ifdef EVENTD_DEBUG
    g_debug("Sending message:\n%s", message);
#endif /* EVENTD_DEBUG */

    if ( self->ws != NULL )
    {
        eventd_ws_connection_send_message(self->context->ws, self->ws, message);
        goto end;
    }

    if ( g_data_output_stream_put_string(self->out, message, NULL, &error) )
        goto end;

    g_warning("Couldn't send message: %s", error->message);
    g_clear_error(&error);
    g_cancellable_cancel(self->cancellable);

end:
    g_free(message);
}

static void
_eventd_evp_client_protocol_event(EventdProtocol *protocol, EventdEvent *event, gpointer user_data)
{
    EventdEvpClient *self = user_data;

#ifdef EVENTD_DEBUG
    g_debug("Received an event (category: %s): %s", eventd_event_get_category(event), eventd_event_get_name(event));
#endif /* EVENTD_DEBUG */

    self->current = event;
    eventd_core_push_event(self->context->core, event);
    self->current = NULL;
}

static void
_eventd_evp_client_protocol_subscribe(EventdProtocol *protocol, GHashTable *categories, gpointer user_data)
{
    EventdEvpClient *self = user_data;

    if ( categories == NULL )
    {
        self->context->subscribe_all = g_list_prepend(self->context->subscribe_all, self);
        self->subscribe_all = self->context->subscribe_all;
        return;
    }

    GHashTableIter iter;
    gchar *category;
    gpointer dummy;
    gchar *key = NULL;
    GList *list = NULL;
    g_hash_table_iter_init(&iter, categories);
    while ( g_hash_table_iter_next(&iter, (gpointer *) &category, &dummy) )
    {
        if ( g_hash_table_lookup_extended(self->context->subscribe_categories, category, (gpointer *) &key, (gpointer *) &list) )
            g_hash_table_steal(self->context->subscribe_categories, category);
        else
            key = g_strdup(category);

        list = g_list_prepend(list, self);

        g_hash_table_insert(self->context->subscribe_categories, key, list);
        g_hash_table_insert(self->subscriptions, key, list);
    }
}

static void
_eventd_evp_client_protocol_bye(EventdProtocol *protocol, const gchar *message, gpointer user_data)
{
    EventdEvpClient *self = user_data;

#ifdef EVENTD_DEBUG
    g_debug("Client connection closed");
#endif /* EVENTD_DEBUG */

    if ( self->ws != NULL )
        eventd_ws_connection_close(self->context->ws, self->ws);
    else
        g_cancellable_cancel(self->cancellable);
}

static const EventdProtocolCallbacks _eventd_evp_client_protocol_callbacks = {
    .event = _eventd_evp_client_protocol_event,
    .subscribe = _eventd_evp_client_protocol_subscribe,
    .bye = _eventd_evp_client_protocol_bye
};

static void
_eventd_evp_client_read_callback(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    EventdEvpClient *self = user_data;
    GError *error = NULL;
    gchar *line;

    line = g_data_input_stream_read_line_finish_utf8(G_DATA_INPUT_STREAM(obj), res, NULL, &error);
    if ( line == NULL )
    {
        if ( ( error == NULL ) || g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED) )
            goto end;
        goto error;
    }
    if ( self->first )
    {
        self->first = FALSE;
        if ( ! g_str_has_prefix(line, "GET /") )
            goto skip_ws;
        const gchar *http_ver;
        if ( g_str_has_suffix(line, " HTTP/1.1\r") )
            http_ver = "HTTP/1.1";
        else if ( g_str_has_suffix(line, " HTTP/1.0\r") )
            http_ver = "HTTP/1.0";
        else
            goto skip_ws;

        if ( self->context->ws == NULL )
        {
            /* The WebSocket module is not loaded, make up some basic HTTP response */
            _eventd_evp_client_send_message(self, g_strdup_printf("%s 501 Not Implemented\r\n\r\n", http_ver));
            goto end;
        }

        GIOStream *stream = G_IO_STREAM(self->connection);
        if ( self->tls != NULL )
            stream = G_IO_STREAM(self->tls);

        self->ws = eventd_ws_connection_server_new(self->context->ws, self, (GDestroyNotify) _eventd_evp_client_disconnect_internal, self->cancellable, stream, self->in, self->protocol, line);

        g_filter_output_stream_set_close_base_stream(G_FILTER_OUTPUT_STREAM(self->out), FALSE);
        g_object_unref(self->out);
        self->in = NULL;
        self->out = NULL;

        g_free(line);
        return;
    }

skip_ws:
    if ( ! eventd_protocol_parse(self->protocol, line, &error) )
        goto error;
    g_free(line);

    return g_data_input_stream_read_line_async(self->in, G_PRIORITY_DEFAULT, self->cancellable, _eventd_evp_client_read_callback, self);

error:
    _eventd_evp_client_send_message(self, eventd_protocol_generate_bye(self->protocol, error->message));
end:
    g_free(line);
    g_clear_error(&error);
    return _eventd_evp_client_disconnect_internal(self);
}


/*
 * Callback for the client connection
 */

static void
_eventd_evp_client_connect(EventdEvpClient *self, GIOStream *stream)
{
    self->in = g_data_input_stream_new(g_io_stream_get_input_stream(stream));
    self->out = g_data_output_stream_new(g_io_stream_get_output_stream(stream));
    g_data_input_stream_set_newline_type(self->in, G_DATA_STREAM_NEWLINE_TYPE_LF);

    g_data_input_stream_read_line_async(self->in, G_PRIORITY_DEFAULT, self->cancellable, _eventd_evp_client_read_callback, self);
}

static void
_eventd_evp_client_tls_handshake_callback(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    EventdEvpClient *self = user_data;
    GError *error = NULL;
    if ( ! g_tls_connection_handshake_finish(G_TLS_CONNECTION(obj), res, &error) )
    {
        g_warning("Could not finish TLS handshake: %s", error->message);
        g_clear_error(&error);
        return _eventd_evp_client_disconnect_internal(self);
    }

    _eventd_evp_client_connect(self, self->tls);
}

gboolean
eventd_evp_client_connection_handler(GSocketService *service, GSocketConnection *connection, GObject *obj, gpointer user_data)
{
    EventdEvpContext *context = user_data;

    GIOStream *tls = NULL;
    if ( G_IS_TCP_CONNECTION(connection) )
    {
        GError *error = NULL;

        GSocketAddress *address;
        address = g_socket_connection_get_remote_address(connection, &error);
        if ( address == NULL )
        {
            g_warning("Could not get client address: %s", error->message);
            g_clear_error(&error);
            return FALSE;
        }

        GInetAddress *inet_address;
        inet_address = g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(address));

        if ( ! g_inet_address_get_is_loopback(inet_address) )
        {
            if ( context->certificate == NULL )
            {
                g_warning("A TLS connection is required (non-loopback TCP) but no TLS certificate");
                return FALSE;
            }

            tls = g_tls_server_connection_new(G_IO_STREAM(connection), context->certificate, &error);
            if ( tls == NULL )
            {
                g_warning("Could not initialize TLS connection: %s", error->message);
                g_clear_error(&error);
                return FALSE;
            }
        }
    }
    EventdEvpClient *self;

    self = g_new0(EventdEvpClient, 1);
    self->context = context;
    self->first = TRUE;

    self->protocol = eventd_protocol_new(&_eventd_evp_client_protocol_callbacks, self, NULL);
    self->subscriptions = g_hash_table_new(g_str_hash, g_str_equal);

    self->cancellable = g_cancellable_new();
    self->connection = g_object_ref(connection);
    self->tls = tls;

    if ( self->tls != NULL )
        g_tls_connection_handshake_async(G_TLS_CONNECTION(tls), G_PRIORITY_DEFAULT, self->cancellable, _eventd_evp_client_tls_handshake_callback, self);
    else
        _eventd_evp_client_connect(self, G_IO_STREAM(self->connection));

    self->link = context->clients = g_list_prepend(context->clients, self);

    return FALSE;
}


static void
_eventd_evp_client_disconnect_internal(EventdEvpClient *self)
{
    GHashTableIter iter;
    gchar *category, *key;
    GList *link, *list;
    g_hash_table_iter_init(&iter, self->subscriptions);
    while ( g_hash_table_iter_next(&iter, (gpointer *) &category, (gpointer *) &link) )
    {
        g_hash_table_iter_steal(&iter);

        g_hash_table_lookup_extended(self->context->subscribe_categories, category, (gpointer *) &key, (gpointer *) &list);
        g_hash_table_steal(self->context->subscribe_categories, category);

        list = g_list_delete_link(list, link);

        g_hash_table_insert(self->context->subscribe_categories, key, list);
    }

    self->context->subscribe_all = g_list_delete_link(self->context->subscribe_all, self->subscribe_all);

    self->context->clients = g_list_remove_link(self->context->clients, self->link);

    eventd_evp_client_disconnect(self);
}

void
eventd_evp_client_disconnect(gpointer data)
{
    EventdEvpClient *self = data;

    g_hash_table_unref(self->subscriptions);

    if ( self->ws != NULL )
        eventd_ws_connection_free(self->context->ws, self->ws);

    if ( self->in != NULL )
        g_object_unref(self->in);
    if ( self->out != NULL )
        g_object_unref(self->out);

    if ( self->tls != NULL )
        g_object_unref(self->tls);

    if ( self->connection != NULL )
    {
        g_io_stream_close(G_IO_STREAM(self->connection), NULL, NULL);
        g_object_unref(self->connection);
    }

    g_object_unref(self->cancellable);
    eventd_protocol_unref(self->protocol);

    g_free(self);
}

void
eventd_evp_client_event_dispatch(EventdEvpClient *self, EventdEvent *event)
{
    if ( self->current == event )
        /* Do not send back our own events */
        return;

    _eventd_evp_client_send_message(self, eventd_protocol_generate_event(self->protocol, event));
}
