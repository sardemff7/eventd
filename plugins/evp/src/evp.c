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

#include "avahi.h"

#ifdef ENABLE_AVAHI
#define AVAHI_OPTION_FLAG 0
#else /* ! ENABLE_AVAHI */
#define AVAHI_OPTION_FLAG G_OPTION_FLAG_HIDDEN
#endif /* ! ENABLE_AVAHI */

struct _EventdPluginContext {
    EventdPluginCoreContext *core;
    EventdPluginCoreInterface *core_interface;
    EventdEvpAvahiContext *avahi;
    gboolean default_unix;
    gchar **binds;
    gboolean no_avahi;
    gchar *avahi_name;
    GSocketService *service;
    GSList *clients;
    GHashTable *events;
};

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
_eventd_evp_error(gpointer data, LibeventdEvpContext *evp, GError *error)
{
    g_warning("Connection error: %s", error->message);
    g_error_free(error);
}

static void
_eventd_evp_event_answered(EventdEvent *event, const gchar *answer, gpointer user_data)
{
    EventdEvpEvent *evp_event = user_data;
    EventdEvpClient *client = evp_event->client;
    GError *error = NULL;

    if ( ! libeventd_evp_context_send_answered(client->evp, evp_event->id, answer, eventd_event_get_all_answer_data(evp_event->event), &error) )
    {
        g_warning("Couldn't send ANSWERED message: %s", error->message);
        g_clear_error(&error);
    }
    g_signal_handler_disconnect(evp_event->event, evp_event->answered_handler);
    evp_event->answered_handler = 0;
}

static void
_eventd_evp_event_ended(EventdEvent *event, EventdEventEndReason reason, gpointer user_data)
{
    EventdEvpEvent *evp_event = user_data;
    EventdEvpClient *client = evp_event->client;
    GError *error = NULL;

    if ( ! libeventd_evp_context_send_ended(client->evp, evp_event->id, reason, &error) )
    {
        g_warning("Couldn't send ENDED message: %s", error->message);
        g_clear_error(&error);
    }
    g_signal_handler_disconnect(evp_event->event, evp_event->ended_handler);
    evp_event->ended_handler = 0;

    g_hash_table_remove(client->events, evp_event->id);
}

static void
_eventd_evp_answered(gpointer data, LibeventdEvpContext *evp, const gchar *id, const gchar *answer, GHashTable *data_hash)
{
    EventdEvpClient *client = data;
    EventdEvpEvent *evp_event;
    evp_event = g_hash_table_lookup(client->events, id);
    if ( evp_event == NULL )
        return;

    if ( data_hash != NULL )
        eventd_event_set_all_answer_data(evp_event->event, g_hash_table_ref(data_hash));
    g_signal_handler_disconnect(evp_event->event, evp_event->answered_handler);
    evp_event->answered_handler = 0;
    eventd_event_answer(evp_event->event, answer);
}

static void
_eventd_evp_ended(gpointer data, LibeventdEvpContext *evp, const gchar *id, EventdEventEndReason reason)
{
    EventdEvpClient *client = data;
    EventdEvpEvent *evp_event;
    evp_event = g_hash_table_lookup(client->events, id);
    if ( evp_event == NULL )
        return;

    g_signal_handler_disconnect(evp_event->event, evp_event->ended_handler);
    evp_event->ended_handler = 0;
    eventd_event_end(evp_event->event, reason);
    g_hash_table_remove(client->events, evp_event->id);
}

static void
_eventd_evp_event(gpointer data, LibeventdEvpContext *evp, const gchar *id, EventdEvent *event)
{
    EventdEvpClient *client = data;
#ifdef DEBUG
    g_debug("Received an event (category: %s): %s", eventd_event_get_category(event), eventd_event_get_name(event));
#endif /* DEBUG */

    if ( ! eventd_plugin_core_push_event(client->context->core, client->context->core_interface, event) )
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
    evp_event->client = client;
    evp_event->id = g_strdup(id);
    evp_event->event = g_object_ref(event);

    g_hash_table_insert(client->events, evp_event->id, evp_event);

    evp_event->answered_handler = g_signal_connect(event, "answered", G_CALLBACK(_eventd_evp_event_answered), evp_event);
    evp_event->ended_handler = g_signal_connect(event, "ended", G_CALLBACK(_eventd_evp_event_ended), evp_event);
}

static void
_eventd_evp_end(gpointer data, LibeventdEvpContext *evp, const gchar *id)
{
    EventdEvpClient *client = data;
    EventdEvpEvent *evp_event;
    evp_event = g_hash_table_lookup(client->events, id);
    if ( evp_event == NULL )
        return;

    eventd_event_end(evp_event->event, EVENTD_EVENT_END_REASON_CLIENT_DISMISS);
}

static void
_eventd_evp_bye(gpointer data, LibeventdEvpContext *evp)
{
    EventdEvpClient *client = data;
    EventdPluginContext *context = client->context;

#ifdef DEBUG
    g_debug("Client connection closed");
#endif /* DEBUG */

    libeventd_evp_context_free(client->evp);
    g_hash_table_unref(client->events);

    context->clients = g_slist_remove(context->clients, client);

    g_free(client);
}

static void
_eventd_evp_event_free(gpointer data)
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

static LibeventdEvpClientInterface _eventd_evp_interface = {
    .error     = _eventd_evp_error,

    .event     = _eventd_evp_event,
    .end       = _eventd_evp_end,

    .answered  = _eventd_evp_answered,
    .ended     = _eventd_evp_ended,

    .bye       = _eventd_evp_bye
};

/*
 * Callback for the client connection
 */
static gboolean
_eventd_service_connection_handler(GSocketService *socket_service, GSocketConnection *connection, GObject *source_object, gpointer user_data)
{
    EventdPluginContext *service = user_data;

    EventdEvpClient *client;

    client = g_new0(EventdEvpClient, 1);
    client->context = service;

    service->clients = g_slist_prepend(service->clients, client);

    client->events = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, _eventd_evp_event_free);
    client->evp = libeventd_evp_context_new_for_connection(client, &_eventd_evp_interface, connection);

    libeventd_evp_context_receive_loop(client->evp, G_PRIORITY_DEFAULT);

    return FALSE;
}


/*
 * Initialization interface
 */

static EventdPluginContext *
_eventd_evp_init(EventdPluginCoreContext *core, EventdPluginCoreInterface *core_interface)
{
    EventdPluginContext *service;

    service = g_new0(EventdPluginContext, 1);

    service->core = core;
    service->core_interface= core_interface;

    service->avahi_name = g_strdup_printf(PACKAGE_NAME " %s", g_get_host_name());

    service->events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    return service;
}

static void
_eventd_evp_uninit(EventdPluginContext *service)
{
    g_hash_table_unref(service->events);

    g_free(service->avahi_name);

    if ( service->binds != NULL )
        g_strfreev(service->binds);

    g_free(service);
}


/*
 * Start/Stop interface
 */

static GList *
_eventd_evp_add_socket(GList *used_sockets, EventdPluginContext *context, const gchar * const *binds)
{
    GList *sockets;
    sockets = eventd_plugin_core_get_sockets(context->core, context->core_interface, binds);

    GList *socket_;
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

    service->service = g_socket_service_new();

    if ( service->binds != NULL )
        sockets = _eventd_evp_add_socket(sockets, service, (const gchar * const *)service->binds);

#ifdef HAVE_GIO_UNIX
    if ( service->default_unix )
    {
        const gchar *binds[] = { "unix-runtime:" EVP_UNIX_SOCKET, NULL };
        sockets = _eventd_evp_add_socket(sockets, service, binds);
    }
#endif /* HAVE_GIO_UNIX */

    g_signal_connect(service->service, "incoming", G_CALLBACK(_eventd_service_connection_handler), service);

#ifdef ENABLE_AVAHI
    if ( ! service->no_avahi )
        service->avahi = eventd_evp_avahi_start(service->avahi_name, sockets);
    else
#endif /* ENABLE_AVAHI */
        g_list_free_full(sockets, g_object_unref);
}

static void
_eventd_service_client_disconnect(gpointer data)
{
    EventdEvpClient *client = data;
    libeventd_evp_context_close(client->evp);
    _eventd_evp_bye(client, client->evp);
}

static void
_eventd_evp_stop(EventdPluginContext *service)
{
#ifdef ENABLE_AVAHI
    eventd_evp_avahi_stop(service->avahi);
#endif /* ENABLE_AVAHI */

    g_slist_free_full(service->clients, _eventd_service_client_disconnect);

    g_socket_service_stop(service->service);
    g_socket_listener_close(G_SOCKET_LISTENER(service->service));
    g_object_unref(service->service);
}


/*
 * Command-line options interface
 */

static GOptionGroup *
_eventd_evp_get_option_group(EventdPluginContext *context)
{
    GOptionGroup *option_group;
    GOptionEntry entries[] =
    {
        { "listen",         'l', 0,                 G_OPTION_ARG_STRING_ARRAY, &context->binds,        "Add a socket to listen to",     "<socket>" },
#ifdef HAVE_GIO_UNIX
        { "listen-default", 'u', 0,                 G_OPTION_ARG_NONE,         &context->default_unix, "Listen on default UNIX socket", NULL },
#endif /* HAVE_GIO_UNIX */
        { "no-avahi",       'A', AVAHI_OPTION_FLAG, G_OPTION_ARG_NONE,         &context->no_avahi,     "Disable avahi publishing",      NULL },
        { NULL }
    };

    option_group = g_option_group_new("event", "EVENT protocol plugin options", "Show EVENT plugin help options", NULL, NULL);
    g_option_group_set_translation_domain(option_group, GETTEXT_PACKAGE);
    g_option_group_add_entries(option_group, entries);
    return option_group;
}


/*
 * Configuration interface
 */

static void
_eventd_evp_global_parse(EventdPluginContext *context, GKeyFile *config_file)
{
    gchar *avahi_name;

    if ( ! g_key_file_has_group(config_file, "Server") )
        return;

    if ( evhelpers_config_key_file_get_string(config_file, "Server", "AvahiName", &avahi_name) < 0 )
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

    r = evhelpers_config_key_file_get_boolean(config_file, "Event", "Disable", &disable);
    if ( r < 0 )
        return;

    if ( disable )
        g_hash_table_insert(context->events, g_strdup(id), GUINT_TO_POINTER(disable));
}

static void
_eventd_evp_config_reset(EventdPluginContext *context)
{
    g_hash_table_remove_all(context->events);
}


/*
 * Plugin interface
 */

EVENTD_EXPORT const gchar *eventd_plugin_id = "eventd-evp";
EVENTD_EXPORT
void
eventd_plugin_get_interface(EventdPluginInterface *interface)
{
    eventd_plugin_interface_add_init_callback(interface, _eventd_evp_init);
    eventd_plugin_interface_add_uninit_callback(interface, _eventd_evp_uninit);

    eventd_plugin_interface_add_get_option_group_callback(interface, _eventd_evp_get_option_group);

    eventd_plugin_interface_add_start_callback(interface, _eventd_evp_start);
    eventd_plugin_interface_add_stop_callback(interface, _eventd_evp_stop);

    eventd_plugin_interface_add_global_parse_callback(interface, _eventd_evp_global_parse);
    eventd_plugin_interface_add_event_parse_callback(interface, _eventd_evp_event_parse);
    eventd_plugin_interface_add_config_reset_callback(interface, _eventd_evp_config_reset);
}
