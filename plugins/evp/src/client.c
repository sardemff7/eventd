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
#include <eventd-plugin.h>
#include <libeventd-evp.h>
#include <libeventd-helpers-config.h>

#include "evp.h"

#include "client.h"

typedef struct {
    EventdPluginContext *context;
    LibeventdEvpContext *evp;
    GHashTable *events;
} EventdEvpClient;

typedef struct {
    EventdEvent *event;
    gulong answered;
    gulong ended;
} EventdEvpEventHandlers;

static void
_eventd_evp_client_event_answered(EventdEvpClient *self, const gchar *answer, EventdEvent *event)
{
    GError *error = NULL;
    EventdEvpEventHandlers *handlers;
    handlers = g_hash_table_lookup(self->events, event);
    g_return_if_fail(handlers != NULL);

    g_signal_handler_disconnect(event, handlers->answered);
    handlers->answered = 0;

    if ( ! libeventd_evp_context_send_answered(self->evp, event, answer, &error) )
    {
        g_warning("Couldn't send ANSWERED message: %s", error->message);
        g_clear_error(&error);
    }
}

static void
_eventd_evp_client_protocol_answered(gpointer data, LibeventdEvpContext *evp, EventdEvent *event, const gchar *answer)
{
    EventdEvpClient *self = data;
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
    GError *error = NULL;

    g_hash_table_remove(self->events, event);
    if ( ! libeventd_evp_context_send_ended(self->evp, event, reason, &error) )
    {
        g_warning("Couldn't send ENDED message: %s", error->message);
        g_clear_error(&error);
    }
}

static void
_eventd_evp_client_protocol_ended(gpointer data, LibeventdEvpContext *evp, EventdEvent *event, EventdEventEndReason reason)
{
    EventdEvpClient *self = data;

    g_hash_table_remove(self->events, event);
    eventd_event_end(event, reason);
}

static void
_eventd_evp_client_protocol_event(gpointer data, LibeventdEvpContext *evp, EventdEvent *event)
{
    EventdEvpClient *self = data;
#ifdef EVENTD_DEBUG
    g_debug("Received an event (category: %s): %s", eventd_event_get_category(event), eventd_event_get_name(event));
#endif /* EVENTD_DEBUG */

    if ( ! eventd_plugin_core_push_event(self->context->core, self->context->core_interface, event) )
    {
        GError *error = NULL;
        if ( ! libeventd_evp_context_send_ended(evp, event, EVENTD_EVENT_END_REASON_RESERVED, &error) )
        {
            g_warning("Couldn't send ENDED message: %s", error->message);
            g_error_free(error);
        }
        return;
    }

    EventdEvpEventHandlers *handlers;

    handlers = g_slice_new(EventdEvpEventHandlers);
    handlers->event = g_object_ref(event);

    handlers->answered = g_signal_connect_swapped(event, "answered", G_CALLBACK(_eventd_evp_client_event_answered), self);
    handlers->ended = g_signal_connect_swapped(event, "ended", G_CALLBACK(_eventd_evp_client_event_ended), self);

    g_hash_table_insert(self->events, event, handlers);
}

static void
_eventd_evp_client_protocol_bye(gpointer data, LibeventdEvpContext *evp)
{
    EventdEvpClient *self = data;
    EventdPluginContext *context = self->context;

#ifdef EVENTD_DEBUG
    g_debug("Client connection closed");
#endif /* EVENTD_DEBUG */

    libeventd_evp_context_free(self->evp);
    g_hash_table_unref(self->events);

    context->clients = g_slist_remove(context->clients, self);

    g_free(self);
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

static LibeventdEvpClientInterface _eventd_evp_client_interface = {
    .event     = _eventd_evp_client_protocol_event,
    .answered  = _eventd_evp_client_protocol_answered,
    .ended     = _eventd_evp_client_protocol_ended,

    .bye       = _eventd_evp_client_protocol_bye
};

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

    context->clients = g_slist_prepend(context->clients, self);

    self->events = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, _eventd_evp_client_event_handlers_free);
    self->evp = libeventd_evp_context_new_for_connection(self, &_eventd_evp_client_interface, connection);

    libeventd_evp_context_receive_loop(self->evp, G_PRIORITY_DEFAULT);

    return FALSE;
}

void
eventd_evp_client_disconnect(gpointer data)
{
    EventdEvpClient *self = data;
    libeventd_evp_context_close(self->evp);
    _eventd_evp_client_protocol_bye(self, self->evp);
}
