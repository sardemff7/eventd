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
    guint64 id;
    GHashTable *events;
} EventdEvpClient;

typedef struct {
    EventdEvpClient *client;
    gchar *id;
    EventdEvent *event;
    gulong answered_handler;
    gulong ended_handler;
} EventdEvpEvent;

static void
_eventd_evp_client_event_answered(EventdEvpEvent *evp_event, const gchar *answer, EventdEvent *event)
{
    EventdEvpClient *self = evp_event->client;
    GError *error = NULL;

    if ( ! libeventd_evp_context_send_answered(self->evp, evp_event->id, answer, evp_event->event, &error) )
    {
        g_warning("Couldn't send ANSWERED message: %s", error->message);
        g_clear_error(&error);
    }
    g_signal_handler_disconnect(evp_event->event, evp_event->answered_handler);
    evp_event->answered_handler = 0;
}

static void
_eventd_evp_client_protocol_answered(gpointer data, LibeventdEvpContext *evp, const gchar *id, const gchar *answer, GHashTable *data_hash)
{
    EventdEvpClient *self = data;
    EventdEvpEvent *evp_event;
    evp_event = g_hash_table_lookup(self->events, id);
    if ( evp_event == NULL )
        return;

    if ( data_hash != NULL )
        eventd_event_set_all_answer_data(evp_event->event, g_hash_table_ref(data_hash));
    g_signal_handler_disconnect(evp_event->event, evp_event->answered_handler);
    evp_event->answered_handler = 0;
    eventd_event_answer(evp_event->event, answer);
}

static void
_eventd_evp_client_event_ended(EventdEvpEvent *evp_event, EventdEventEndReason reason, EventdEvent *event)
{
    EventdEvpClient *self = evp_event->client;
    GError *error = NULL;

    if ( ! libeventd_evp_context_send_ended(self->evp, evp_event->id, reason, &error) )
    {
        g_warning("Couldn't send ENDED message: %s", error->message);
        g_clear_error(&error);
    }
    g_signal_handler_disconnect(evp_event->event, evp_event->ended_handler);
    evp_event->ended_handler = 0;

    g_hash_table_remove(self->events, evp_event->id);
}

static void
_eventd_evp_client_protocol_ended(gpointer data, LibeventdEvpContext *evp, const gchar *id, EventdEventEndReason reason)
{
    EventdEvpClient *self = data;
    EventdEvpEvent *evp_event;
    evp_event = g_hash_table_lookup(self->events, id);
    if ( evp_event == NULL )
        return;

    g_signal_handler_disconnect(evp_event->event, evp_event->ended_handler);
    evp_event->ended_handler = 0;
    eventd_event_end(evp_event->event, reason);
    g_hash_table_remove(self->events, evp_event->id);
}

static void
_eventd_evp_client_protocol_event(gpointer data, LibeventdEvpContext *evp, const gchar *id, EventdEvent *event)
{
    EventdEvpClient *self = data;
#ifdef EVENTD_DEBUG
    g_debug("Received an event (category: %s): %s", eventd_event_get_category(event), eventd_event_get_name(event));
#endif /* EVENTD_DEBUG */

    if ( ! eventd_plugin_core_push_event(self->context->core, self->context->core_interface, event) )
    {
        GError *error = NULL;
        if ( ! libeventd_evp_context_send_ended(evp, id, EVENTD_EVENT_END_REASON_RESERVED, &error) )
        {
            g_warning("Couldn't send ENDED message: %s", error->message);
            g_error_free(error);
        }
        return;
    }

    EventdEvpEvent *evp_event;

    evp_event = g_new0(EventdEvpEvent, 1);
    evp_event->client = self;
    evp_event->id = g_strdup(id);
    evp_event->event = g_object_ref(event);

    g_hash_table_insert(self->events, evp_event->id, evp_event);

    evp_event->answered_handler = g_signal_connect_swapped(event, "answered", G_CALLBACK(_eventd_evp_client_event_answered), evp_event);
    evp_event->ended_handler = g_signal_connect_swapped(event, "ended", G_CALLBACK(_eventd_evp_client_event_ended), evp_event);
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
_eventd_evp_client_event_free(gpointer data)
{
    EventdEvpEvent *evp_event = data;

    if ( evp_event->answered_handler > 0 )
        g_signal_handler_disconnect(evp_event->event, evp_event->answered_handler);
    if ( evp_event->ended_handler > 0 )
        g_signal_handler_disconnect(evp_event->event, evp_event->ended_handler);

    g_object_unref(evp_event->event);
    g_free(evp_event->id);

    g_free(evp_event);
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
eventd_evp_client_connection_handler(GSocketService *service, GSocketConnection *connection, GObject *source_object, gpointer user_data)
{
    EventdPluginContext *context = user_data;

    EventdEvpClient *self;

    self = g_new0(EventdEvpClient, 1);
    self->context = context;

    context->clients = g_slist_prepend(context->clients, self);

    self->events = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, _eventd_evp_client_event_free);
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
