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

#include <config.h>

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <libeventd-event.h>
#include <libeventd-protocol.h>
#include <eventd-plugin.h>
#include <libeventd-helpers-config.h>

#include "evp.h"

#include "client.h"

typedef struct {
    EventdPluginContext *context;
    EventdProtocol *protocol;
    GCancellable *cancellable;
    GSocketConnection *connection;
    GDataInputStream *in;
    GDataOutputStream *out;
    GError *error;
    GHashTable *events;
} EventdEvpClient;

typedef struct {
    EventdEvent *event;
    gulong answered;
    gulong ended;
} EventdEvpEventHandlers;

static void
_eventd_evp_client_send_message(EventdEvpClient *self, gchar *message)
{
    GError *error = NULL;

#ifdef EVENTD_DEBUG
    g_debug("Sending message:\n%s", message);
#endif /* EVENTD_DEBUG */

    if ( g_data_output_stream_put_string(self->out, message, NULL, &error) )
        goto end;

    g_warning("Couldn't send message: %s", error->message);
    g_clear_error(&error);
    g_cancellable_cancel(self->cancellable);

end:
    g_free(message);
}

static void
_eventd_evp_client_event_answered(EventdEvpClient *self, const gchar *answer, EventdEvent *event)
{
    EventdEvpEventHandlers *handlers;
    handlers = g_hash_table_lookup(self->events, event);
    g_return_if_fail(handlers != NULL);

    g_signal_handler_disconnect(event, handlers->answered);
    handlers->answered = 0;
    _eventd_evp_client_send_message(self, eventd_protocol_generate_answered(self->protocol, event, answer));
}

static void
_eventd_evp_client_protocol_answered(EventdEvpClient *self, EventdEvent *event, const gchar *answer, EventdProtocol *protocol)
{
    EventdEvpEventHandlers *handlers;
    handlers = g_hash_table_lookup(self->events, event);
    g_return_if_fail(handlers != NULL);

    g_signal_handler_disconnect(event, handlers->answered);
    handlers->answered = 0;
    eventd_event_answer(event, answer);
}

static void
_eventd_evp_client_event_ended(EventdEvpClient *self, EventdEventEndReason reason, EventdEvent *event)
{
    g_hash_table_remove(self->events, event);
    _eventd_evp_client_send_message(self, eventd_protocol_generate_ended(self->protocol, event, reason));
}

static void
_eventd_evp_client_protocol_ended(EventdEvpClient *self, EventdEvent *event, EventdEventEndReason reason, EventdProtocol *protocol)
{
    g_hash_table_remove(self->events, event);
    eventd_event_end(event, reason);
}

static void
_eventd_evp_client_protocol_event(EventdEvpClient *self, EventdEvent *event, EventdProtocol *protocol)
{
#ifdef EVENTD_DEBUG
    g_debug("Received an event (category: %s): %s", eventd_event_get_category(event), eventd_event_get_name(event));
#endif /* EVENTD_DEBUG */

    if ( ! eventd_plugin_core_push_event(self->context->core, self->context->core_interface, event) )
    {
        if ( self->out != NULL )
            /* Client not in passive mode */
            _eventd_evp_client_send_message(self, eventd_protocol_generate_ended(self->protocol, event, EVENTD_EVENT_END_REASON_RESERVED));
        return;
    }

    if ( self->out == NULL )
        /* Client in passive mode */
        return;

    EventdEvpEventHandlers *handlers;

    handlers = g_slice_new(EventdEvpEventHandlers);
    handlers->event = g_object_ref(event);

    handlers->answered = g_signal_connect_swapped(event, "answered", G_CALLBACK(_eventd_evp_client_event_answered), self);
    handlers->ended = g_signal_connect_swapped(event, "ended", G_CALLBACK(_eventd_evp_client_event_ended), self);

    g_hash_table_insert(self->events, event, handlers);
}

static void
_eventd_evp_client_protocol_bye(EventdEvpClient *self, EventdProtocol *protocol)
{
#ifdef EVENTD_DEBUG
    g_debug("Client connection closed");
#endif /* EVENTD_DEBUG */

    g_cancellable_cancel(self->cancellable);
}

static void
_eventd_evp_client_protocol_passive(EventdEvpClient *self, EventdProtocol *protocol)
{
    if ( self->out == NULL )
        return eventd_evp_client_disconnect(self);
    g_object_unref(self->out);
    self->out = NULL;
}

static void
_eventd_evp_client_event_handlers_free(gpointer data)
{
    EventdEvpEventHandlers *handlers = data;

    if ( handlers->answered > 0 )
        g_signal_handler_disconnect(handlers->event, handlers->answered);
    g_signal_handler_disconnect(handlers->event, handlers->ended);

    g_object_unref(handlers->event);

    g_slice_free(EventdEvpEventHandlers, handlers);
}

static void
_eventd_evp_client_read_callback(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    EventdEvpClient *self = user_data;
    gchar *line;

    line = g_data_input_stream_read_line_finish_utf8(G_DATA_INPUT_STREAM(obj), res, NULL, &self->error);
    if ( line == NULL )
    {
        if ( ( self->error != NULL ) && ( self->error->code == G_IO_ERROR_CANCELLED ) )
            g_clear_error(&self->error);
        return eventd_evp_client_disconnect(self);
    }

    if ( ! eventd_protocol_parse(self->protocol, &line, &self->error) )
        return;

    g_data_input_stream_read_line_async(self->in, G_PRIORITY_DEFAULT, self->cancellable, _eventd_evp_client_read_callback, self);
}

/*
 * Callback for the self connection
 */
gboolean
eventd_evp_client_connection_handler(GSocketService *service, GSocketConnection *connection, GObject *obj, gpointer user_data)
{
    EventdPluginContext *context = user_data;

    EventdEvpClient *self;

    self = g_new0(EventdEvpClient, 1);
    self->context = context;

    self->protocol = eventd_protocol_evp_new();
    self->events = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, _eventd_evp_client_event_handlers_free);

    self->cancellable = g_cancellable_new();
    self->connection = g_object_ref(connection);
    self->in = g_data_input_stream_new(g_io_stream_get_input_stream(G_IO_STREAM(connection)));
    self->out = g_data_output_stream_new(g_io_stream_get_output_stream(G_IO_STREAM(connection)));

    g_data_input_stream_set_newline_type(self->in, G_DATA_STREAM_NEWLINE_TYPE_LF);

    g_data_input_stream_read_line_async(self->in, G_PRIORITY_DEFAULT, self->cancellable, _eventd_evp_client_read_callback, self);

    g_signal_connect_swapped(self->protocol, "event", G_CALLBACK(_eventd_evp_client_protocol_event), self);
    g_signal_connect_swapped(self->protocol, "answered", G_CALLBACK(_eventd_evp_client_protocol_answered), self);
    g_signal_connect_swapped(self->protocol, "ended", G_CALLBACK(_eventd_evp_client_protocol_ended), self);
    g_signal_connect_swapped(self->protocol, "bye", G_CALLBACK(_eventd_evp_client_protocol_bye), self);
    g_signal_connect_swapped(self->protocol, "passive", G_CALLBACK(_eventd_evp_client_protocol_passive), self);

    context->clients = g_slist_prepend(context->clients, self);

    return FALSE;
}

void
eventd_evp_client_disconnect(gpointer data)
{
    EventdEvpClient *self = data;

    g_object_unref(self->in);
    if ( self->out != NULL )
        g_object_unref(self->out);

    if ( self->connection != NULL )
    {
        g_io_stream_close(G_IO_STREAM(self->connection), NULL, NULL);
        g_object_unref(self->connection);
    }

    g_hash_table_unref(self->events);
    g_object_unref(self->protocol);

    self->context->clients = g_slist_remove(self->context->clients, self);

    g_free(self);
}
