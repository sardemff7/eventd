/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011 Quentin "Sardem FF7" Glidic
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
#include <gio/gio.h>

#include <libeventd-client.h>
#include <libeventd-event.h>
#include <eventd-plugin.h>

#include "types.h"

#include "config.h"
#include "queue.h"

#include "dbus.h"

#define NOTIFICATION_BUS_NAME      "org.freedesktop.Notifications"
#define NOTIFICATION_BUS_PATH      "/org/freedesktop/Notifications"

#define NOTIFICATION_SPEC_VERSION  "1.2"

struct _EventdDbusContext {
    EventdQueue *queue;
    EventdConfig *config;
    GDBusNodeInfo *introspection_data;
    guint id;
    GDBusConnection *connection;
    GHashTable *events;
};

typedef struct {
    EventdDbusContext *context;
    gchar *sender;
    EventdEvent *event;
} EventdDbusNotification;

static void
_eventd_dbus_event_ended(EventdEvent *event, EventdEventEndReason reason, EventdDbusNotification *notification)
{
    guint32 id;

    id = eventd_event_get_id(event);
    g_dbus_connection_emit_signal(notification->context->connection, notification->sender,
                                  NOTIFICATION_BUS_PATH, NOTIFICATION_BUS_NAME,
                                  "NotificationClosed", g_variant_new("(uu)", id, reason),
                                  NULL);
    g_hash_table_remove(notification->context->events, GUINT_TO_POINTER(id));
}

static void
_eventd_dbus_notification_free(gpointer user_data)
{
    EventdDbusNotification *notification = user_data;

    g_object_unref(notification->event);

    g_free(notification->sender);

    g_free(notification);
}
static void
_eventd_dbus_notification_new(EventdDbusContext *context, const gchar *sender, guint32 id, EventdEvent *event)
{
    EventdDbusNotification *notification;

    notification = g_new0(EventdDbusNotification, 1);
    notification->context = context;
    notification->sender = g_strdup(sender);
    notification->event = event;

    g_hash_table_insert(context->events, GUINT_TO_POINTER(id), notification);
    g_signal_connect(event, "ended", G_CALLBACK(_eventd_dbus_event_ended), notification);
}

static void
_eventd_dbus_notify(EventdDbusContext *context, const gchar *sender, GVariant *parameters, GDBusMethodInvocation *invocation)
{
    const gchar *app_name;
    guint32 id;
    const gchar *icon;
    const gchar *summary;
    const gchar *body;
    const gchar **actions;
    GVariantIter *hints;
    gchar *hint_name;
    GVariant *hint;
    const gchar *event_name = "libnotify";
    gint16 urgency = -1;

    gint timeout;
    gboolean disable;
    gint64 server_timeout;

    EventdClient *client = NULL;
    EventdEvent *event = NULL;

    client = libeventd_client_new();
    libeventd_client_set_mode(client, EVENTD_CLIENT_MODE_NORMAL);

    g_variant_get(parameters, "(&su&s&s&s^a&sa{sv}i)",
                  &app_name,
                  &id,
                  &icon,
                  &summary,
                  &body,
                  &actions,
                  &hints,
                  &timeout);

    #if DEBUG
    g_debug("Received notification from '%s': '%s'", app_name, summary);
    #endif /* DEBUG */

    while ( g_variant_iter_next(hints, "{&sv}", &hint_name, &hint) )
    {
        #if DEBUG
        g_debug("Found hint '%s'", hint_name);
        #endif /* DEBUG */

        if ( g_strcmp0(hint_name, "urgency") == 0 )
            urgency = g_variant_get_byte(hint);
        else if ( g_strcmp0(hint_name, "category") == 0 )
            event_name = g_variant_get_string(hint, NULL);
        else if ( ( g_strcmp0(hint_name, "image-data") == 0 )
                || ( g_strcmp0(hint_name, "image_data") == 0 ) )
            {}
        else if ( ( g_strcmp0(hint_name, "icon-data") == 0 )
                || ( g_strcmp0(hint_name, "icon_data") == 0 ) )
            {}
        else if ( g_strcmp0(hint_name, "sound-file") == 0 )
            {}

        g_variant_unref(hint);
    }

    #if DEBUG
    g_debug("Creanting event '%s' for client '%s' ", event_name, app_name);
    #endif /* DEBUG */

    libeventd_client_update(client, "libnotify", app_name);
    if ( id == 0 )
        id = eventd_queue_get_next_event_id(context->queue);
    event = eventd_event_new_with_id(id, event_name);

    eventd_config_event_get_disable_and_timeout(context->config, client, event, &disable, &server_timeout);
    if ( disable )
    {
        g_dbus_method_invocation_return_dbus_error(invocation, NOTIFICATION_BUS_NAME ".Disabled", "Notification type disabled");
        g_object_unref(event);
        goto out;
    }

    eventd_event_add_data(event, g_strdup("summary"), g_strdup(summary));

    if ( body != NULL )
        eventd_event_add_data(event, g_strdup("body"), g_strdup(body));

    if ( ( icon != NULL ) && ( *icon != 0 ) )
    {
        #if DEBUG
        g_debug("Icon specified: '%s'", icon);
        #endif /* DEBUG */

        if ( g_str_has_prefix(icon, "file://") )
            eventd_event_add_data(event, g_strdup("overlay-icon"), g_strdup(icon+7));
        else
        {
            /*
             * TODO: Support freedesktop themes
             */
        }
    }

    eventd_event_set_timeout(event, ( timeout > -1 ) ? timeout : ( urgency > -1 ) ? ( 3000 + urgency * 2000 ) : server_timeout);

    _eventd_dbus_notification_new(context, sender, id, event);
    eventd_queue_push(context->queue, client, event);

    g_dbus_method_invocation_return_value(invocation, g_variant_new("(u)", id));

out:
    libeventd_client_unref(client);
}

static void
_eventd_dbus_close_notification(EventdDbusContext *context, GVariant *parameters, GDBusMethodInvocation *invocation)
{
    guint32 id;
    EventdEvent *event;

    g_variant_get(parameters, "(u)", &id);

    if ( id == 0 )
    {
        g_dbus_method_invocation_return_dbus_error(invocation, NOTIFICATION_BUS_NAME ".InvalidId", "Invalid notification identifier");
        return;
    }

    event = g_hash_table_lookup(context->events, GUINT_TO_POINTER(id));

    if ( event != NULL )
        eventd_event_end(event, EVENTD_EVENT_END_REASON_CLIENT_DISMISS);

    g_dbus_method_invocation_return_value(invocation, NULL);
}

static void
_eventd_dbus_get_capabilities(GDBusMethodInvocation *invocation)
{
    GVariantBuilder *builder;

    builder = g_variant_builder_new(G_VARIANT_TYPE("as"));

    g_variant_builder_add(builder, "s", "body");
    g_variant_builder_add(builder, "s", "body-markup");

    /* TODO: quite easy, should be done soon
    g_variant_builder_add(builder, "s", "icon-static");
    */

    /* TODO: make the notification plugin works fine with these
    g_variant_builder_add(builder, "s", "body-hyperlinks");
    */

    /* TODO: pass the data to the sound plugin
    g_variant_builder_add(builder, "s", "sound");
    */

    /* TODO: or not, these imply “some” work
    g_variant_builder_add(builder, "s", "persistence");
    g_variant_builder_add(builder, "s", "actions");
    g_variant_builder_add(builder, "s", "action-icons");
    */

    g_dbus_method_invocation_return_value(invocation, g_variant_new("(as)", builder));
    g_variant_builder_unref(builder);
}

static void
_eventd_dbus_get_server_information(GDBusMethodInvocation *invocation)
{
    g_dbus_method_invocation_return_value(invocation, g_variant_new("(ssss)", PACKAGE_NAME, "Quentin 'Sardem FF7' Glidic", PACKAGE_VERSION, NOTIFICATION_SPEC_VERSION));
}

static void
_eventd_dbus_method(GDBusConnection       *connection,
                    const char            *sender,
                    const char            *object_path,
                    const char            *interface_name,
                    const char            *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
    EventdDbusContext *context = user_data;

    if ( g_strcmp0(method_name, "Notify") == 0 )
        _eventd_dbus_notify(context, sender, parameters, invocation);
    else if ( g_strcmp0(method_name, "CloseNotification") == 0 )
        _eventd_dbus_close_notification(context, parameters, invocation);
    else if ( g_strcmp0(method_name, "GetCapabilities") == 0 )
        _eventd_dbus_get_capabilities(invocation);
    else if ( g_strcmp0(method_name, "GetServerInformation") == 0 )
        _eventd_dbus_get_server_information(invocation);
}

static const gchar introspection_xml[] =
"<node>"
"    <interface name='org.freedesktop.Notifications'>"
"        <method name='Notify'>"
"            <arg type='s' name='app_name' direction='in' />"
"            <arg type='u' name='id' direction='in' />"
"            <arg type='s' name='icon' direction='in' />"
"            <arg type='s' name='summary' direction='in' />"
"            <arg type='s' name='body' direction='in' />"
"            <arg type='as' name='actions' direction='in' />"
"            <arg type='a{sv}' name='hints' direction='in' />"
"            <arg type='i' name='timeout' direction='in' />"
"            <arg type='u' name='return_id' direction='out' />"
"        </method>"
"        <method name='CloseNotification'>"
"            <arg type='u' name='id' direction='in' />"
"        </method>"
"        <method name='GetCapabilities'>"
"            <arg type='as' name='return_caps' direction='out' />"
"        </method>"
"        <method name='GetServerInformation'>"
"            <arg type='s' name='return_name' direction='out' />"
"            <arg type='s' name='return_vendor' direction='out' />"
"            <arg type='s' name='return_version' direction='out' />"
"            <arg type='s' name='return_spec_version' direction='out' />"
"        </method>"
"    </interface>"
"</node>";

static GDBusInterfaceVTable interface_vtable = {
    .method_call = _eventd_dbus_method
};

static void
_eventd_dbus_on_bus_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    EventdDbusContext *context = user_data;
    GError *error = NULL;
    guint object_id;

    context->connection = connection;

    object_id = g_dbus_connection_register_object(connection, NOTIFICATION_BUS_PATH, context->introspection_data->interfaces[0], &interface_vtable, context, NULL, &error);
    if ( object_id == 0 )
    {
        g_warning("Couldn’t register object: %s", error->message);
        g_clear_error(&error);
    }
}

static void
_eventd_dbus_on_name_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    g_debug("Acquired the name %s on the session bus\n", name);
}

static void
_eventd_dbus_on_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    g_warning("Lost DBus name");
}

EventdDbusContext *
eventd_dbus_start(EventdConfig *config, EventdQueue *queue)
{
    EventdDbusContext *context;
    GError *error = NULL;
    GDBusNodeInfo *introspection_data;

    introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, &error);
    if ( introspection_data == NULL )
    {
        g_warning("Couldn’t generate introspection data: %s", error->message);
        g_clear_error(&error);
        return NULL;
    }

    context = g_new0(EventdDbusContext, 1);

    context->introspection_data = introspection_data;

    context->config = config;
    context->queue = queue;

    context->id = g_bus_own_name(G_BUS_TYPE_SESSION, NOTIFICATION_BUS_NAME, G_BUS_NAME_OWNER_FLAGS_NONE, _eventd_dbus_on_bus_acquired, _eventd_dbus_on_name_acquired, _eventd_dbus_on_name_lost, context, NULL);

    context->events = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, _eventd_dbus_notification_free);

    return context;
}

void
eventd_dbus_stop(EventdDbusContext *context)
{
    if ( context == NULL )
        return;

    g_hash_table_unref(context->events);

    g_bus_unown_name(context->id);

    g_free(context);
}
