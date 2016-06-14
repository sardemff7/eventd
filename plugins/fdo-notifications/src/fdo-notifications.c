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

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <glib.h>
#include <gio/gio.h>

#include <libeventd-event.h>
#include <eventd-plugin.h>
#include <libeventd-helpers-dirs.h>

#define NOTIFICATION_BUS_NAME      "org.freedesktop.Notifications"
#define NOTIFICATION_BUS_PATH      "/org/freedesktop/Notifications"

#define NOTIFICATION_SPEC_VERSION  "1.2"

typedef enum {
    EVENTD_FDO_NOTIFICATIONS_CLOSE_REASON_EXPIRED = 1,
    EVENTD_FDO_NOTIFICATIONS_CLOSE_REASON_DISMISS = 2,
    EVENTD_FDO_NOTIFICATIONS_CLOSE_REASON_CALL = 3,
    EVENTD_FDO_NOTIFICATIONS_CLOSE_REASON_RESERVED = 4,
} EventdFdoNotificationsCloseReason;

struct _EventdPluginContext {
    EventdPluginCoreContext *core;
    GDBusNodeInfo *introspection_data;
    guint id;
    GDBusConnection *connection;
    guint object_id;
    GVariant *capabilities;
    GVariant *server_information;
    GRegex *regex_amp;
    GRegex *regex_markup;
    guint32 count;
    GHashTable *ids;
    GHashTable *notifications;
};

typedef struct {
    EventdPluginContext *context;
    guint32 id;
    gchar *sender;
    EventdEvent *event;
    gulong timeout;
} EventdDbusNotification;

static void
_eventd_fdo_notifications_notification_closed(EventdDbusNotification *notification, EventdFdoNotificationsCloseReason reason)
{
    /*
     * We have to emit the NotificationClosed signal for our D-Bus client
     */
    g_dbus_connection_emit_signal(notification->context->connection, notification->sender,
                                  NOTIFICATION_BUS_PATH, NOTIFICATION_BUS_NAME,
                                  "NotificationClosed", g_variant_new("(uu)", notification->id, reason),
                                  NULL);

    if ( notification->timeout > 0 )
        g_source_remove(notification->timeout);
    g_hash_table_remove(notification->context->notifications, eventd_event_get_uuid(notification->event));
}

static gboolean
_eventd_fdo_notifications_notification_timout(gpointer user_data)
{
    EventdDbusNotification *notification = user_data;
    notification->timeout = 0;
    _eventd_fdo_notifications_notification_closed(notification, EVENTD_FDO_NOTIFICATIONS_CLOSE_REASON_RESERVED);
    return FALSE;
}

/*
 * Helper functions
 */

static EventdDbusNotification *
_eventd_fdo_notifications_notification_new(EventdPluginContext *context, const gchar *sender, EventdEvent *event)
{
    if ( ! eventd_plugin_core_push_event(context->core, event) )
        return 0;

    EventdDbusNotification *notification;

    notification = g_new0(EventdDbusNotification, 1);
    notification->context = context;
    notification->id = ++context->count;
    notification->sender = g_strdup(sender);
    notification->event = event;

    eventd_event_add_data(event, g_strdup("libnotify-id"), g_variant_new_uint64(notification->id));

    g_hash_table_insert(context->notifications, (gpointer) eventd_event_get_uuid(event), notification);
    g_hash_table_insert(context->ids, GUINT_TO_POINTER(notification->id), notification);

    return notification;
}

static void
_eventd_fdo_notifications_notification_free(gpointer user_data)
{
    EventdDbusNotification *notification = user_data;

    g_hash_table_remove(notification->context->ids, GUINT_TO_POINTER(notification->id));
    eventd_event_unref(notification->event);

    g_free(notification->sender);

    g_free(notification);
}

static gboolean
_eventd_fdo_notifications_body_try_parse(const gchar *body)
{
    GMarkupParser parser = { NULL };
    GMarkupParseContext *context;
    gboolean ret;
    context = g_markup_parse_context_new(&parser, 0, NULL, NULL);
    ret = g_markup_parse_context_parse(context, body, -1, NULL) && g_markup_parse_context_end_parse(context, NULL);
    g_markup_parse_context_free(context);
    return ret;
}

static gchar *
_eventd_fdo_notifications_body_escape(EventdPluginContext *context, const gchar *body)
{
    GError *error = NULL;
    gchar *escaped, *tmp = NULL;

    escaped = g_regex_replace_literal(context->regex_amp, body, -1, 0, "&amp;" , 0, &error);
    if ( escaped == NULL )
    {
        g_warning("Couldn't escape amp: %s", error->message);
        goto fallback;
    }

    escaped = g_regex_replace_literal(context->regex_markup, tmp = escaped, -1, 0, "&lt;" , 0, &error);
    if ( escaped == NULL )
    {
        g_warning("Couldn't escape markup: %s", error->message);
        goto fallback;
    }
    g_free(tmp);

    if ( ! _eventd_fdo_notifications_body_try_parse(tmp = escaped) )
        goto fallback;

    return escaped;

fallback:
    g_free(tmp);
    g_clear_error(&error);
    return g_markup_escape_text(body, -1);
}


/*
 * D-Bus methods functions
 */

static void
_eventd_fdo_notifications_notify(EventdPluginContext *context, const gchar *sender, GVariant *parameters, GDBusMethodInvocation *invocation)
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
    const gchar *event_name = "generic";
    const gchar *desktop_entry = NULL;
    GVariant *image_data = NULL;
    gint8 urgency = -1;
    const gchar *image_path = NULL;
    const gchar *sound_name = NULL;
    const gchar *sound_file = NULL;
    gboolean no_sound = FALSE;
    gboolean action_icons = FALSE;
    gboolean resident = FALSE;
    gboolean transient = FALSE;

    gint timeout;

    EventdEvent *event = NULL;

    g_variant_get(parameters, "(&su&s&s&s^a&sa{sv}i)",
                  &app_name,
                  &id,
                  &icon,
                  &summary,
                  &body,
                  &actions,
                  &hints,
                  &timeout);

    if ( ( actions != NULL ) && ( ( g_strv_length((gchar **) actions) % 2 ) != 0 ) )
    {
        g_dbus_method_invocation_return_dbus_error(invocation, NOTIFICATION_BUS_NAME ".InvalidActionsArray", "Invalid actions array: actions must be a list of pairs");
        return;
    }

#ifdef EVENTD_DEBUG
    g_debug("Received notification from '%s': '%s'", app_name, summary);
#endif /* EVENTD_DEBUG */

    while ( g_variant_iter_next(hints, "{&sv}", &hint_name, &hint) )
    {
#ifdef EVENTD_DEBUG
        g_debug("Found hint '%s'", hint_name);
#endif /* EVENTD_DEBUG */

        if ( g_strcmp0(hint_name, "category") == 0 )
            event_name = g_variant_get_string(hint, NULL);
        else if ( g_strcmp0(hint_name, "desktop-entry") == 0 )
            desktop_entry = g_variant_get_string(hint, NULL);
        else if ( ( g_strcmp0(hint_name, "image-data") == 0 )
                  || ( g_strcmp0(hint_name, "image_data") == 0 ) )
            image_data = g_variant_ref(hint);
        else if ( ( g_strcmp0(hint_name, "image-path") == 0 )
                  || ( g_strcmp0(hint_name, "image_path") == 0 ) )
            image_path = g_variant_get_string(hint, NULL);
        else if ( ( g_strcmp0(hint_name, "icon_data") == 0 )
                  && image_data == NULL )
            image_data = g_variant_ref(hint);
        else if ( g_strcmp0(hint_name, "urgency") == 0 )
            urgency = g_variant_get_byte(hint);
        else if ( g_strcmp0(hint_name, "sound-name") == 0 )
            sound_name = g_variant_get_string(hint, NULL);
        else if ( g_strcmp0(hint_name, "sound-file") == 0 )
            sound_file = g_variant_get_string(hint, NULL);
        else if ( g_strcmp0(hint_name, "suppress-sound") == 0 )
            no_sound = g_variant_get_boolean(hint);
        else if ( g_strcmp0(hint_name, "action-icons") == 0 )
            action_icons = g_variant_get_boolean(hint);
        else if ( g_strcmp0(hint_name, "resident") == 0 )
            resident = g_variant_get_boolean(hint);
        else if ( g_strcmp0(hint_name, "transient") == 0 )
            transient = g_variant_get_boolean(hint);

        g_variant_unref(hint);
    }

#ifdef EVENTD_DEBUG
    g_debug("Creating event '%s' for client '%s' ", event_name, app_name);
#endif /* EVENTD_DEBUG */

    EventdDbusNotification *notification = NULL;
    if ( id > 0 )
    {
        notification = g_hash_table_lookup(context->notifications, GUINT_TO_POINTER(id));
        if ( notification == NULL )
        {
            g_dbus_method_invocation_return_dbus_error(invocation, NOTIFICATION_BUS_NAME ".InvalidId", "Invalid notification identifier");
            return;
        }
        event = notification->event;
    }
    else
        event = eventd_event_new("notification", event_name);

    eventd_event_add_data_string(event, g_strdup("client-name"), g_strdup(app_name));

    eventd_event_add_data_string(event, g_strdup("title"), g_markup_escape_text(summary, -1));

    if ( body != NULL )
        eventd_event_add_data_string(event, g_strdup("message"), _eventd_fdo_notifications_body_escape(context, body));

    if ( ( icon != NULL ) && ( *icon != 0 ) )
    {
#ifdef EVENTD_DEBUG
        g_debug("Icon specified: '%s'", icon);
#endif /* EVENTD_DEBUG */

        if ( g_str_has_prefix(icon, "file://") )
            eventd_event_add_data_string(event, g_strdup("icon"), g_strdup(icon));
        else
        {
            /*
             * TODO: Support freedesktop themes
             */
        }
    }

    if ( desktop_entry != NULL )
        eventd_event_add_data_string(event, g_strdup("desktop-entry"), g_strdup(desktop_entry));


    if ( image_data != NULL )
    {
        eventd_event_add_data(event, g_strdup("image"), g_variant_new("(msmsv)", "image/x.eventd.gdkpixbuf", NULL, image_data));
        g_variant_unref(image_data);
    }
    else if ( image_path != NULL )
    {
        if ( g_str_has_prefix(image_path, "file://") )
            eventd_event_add_data_string(event, g_strdup("image"), g_strdup(image_path));
    }

    if ( urgency > -1 )
            eventd_event_add_data(event, g_strdup("urgency"), g_variant_new_byte(urgency));

    if ( ! no_sound )
    {
        if ( sound_name != NULL )
            eventd_event_add_data_string(event, g_strdup("sound-name"), g_strdup(sound_name));
        else if ( sound_file != NULL )
            eventd_event_add_data_string(event, g_strdup("sound"), g_strdup_printf("file://%s", sound_file));
    }
    else
        eventd_event_add_data(event, g_strdup("no-sound"), g_variant_new_boolean(TRUE));

    if ( actions != NULL )
    {
        const gchar **action;
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{ss}"));
        for ( action = actions ; *action != NULL ; action += 2 )
            g_variant_builder_add(&builder, "{ss}", *action, *(action + 1));
        eventd_event_add_data(event, g_strdup("actions"), g_variant_builder_end(&builder));
        if ( action_icons )
            eventd_event_add_data(event, g_strdup("action-icons"), g_variant_new_boolean(TRUE));
    }

    if ( resident )
        eventd_event_add_data(event, g_strdup("resident"), g_variant_new_boolean(TRUE));
    if ( transient )
        eventd_event_add_data(event, g_strdup("transient"), g_variant_new_boolean(TRUE));

    if ( id > 0 )
    {
        if ( ! eventd_plugin_core_push_event(context->core, event) )
        {
            _eventd_fdo_notifications_notification_closed(notification, EVENTD_FDO_NOTIFICATIONS_CLOSE_REASON_RESERVED);
            g_dbus_method_invocation_return_dbus_error(invocation, NOTIFICATION_BUS_NAME ".StrangeError", "We have something strange, really");
            return;
        }
    }
    else
    {
        notification = _eventd_fdo_notifications_notification_new(context, sender, event);
        if ( notification == NULL )
        {
            eventd_event_unref(event);
            g_dbus_method_invocation_return_dbus_error(invocation, NOTIFICATION_BUS_NAME ".InvalidNotification", "Invalid notification type");
            return;
        }
    }

    g_dbus_method_invocation_return_value(invocation, g_variant_new("(u)", id));

    if ( notification->timeout > 0 )
        g_source_remove(notification->timeout);
    notification->timeout = g_timeout_add_seconds_full(G_PRIORITY_DEFAULT, 30, _eventd_fdo_notifications_notification_timout, notification, NULL);
}

static void
_eventd_fdo_notifications_close_notification(EventdPluginContext *context, GVariant *parameters, GDBusMethodInvocation *invocation)
{
    g_dbus_method_invocation_return_value(invocation, NULL);
    guint32 id;
    EventdDbusNotification *notification;

    g_variant_get(parameters, "(u)", &id);

    if ( id == 0 )
    {
        g_dbus_method_invocation_return_dbus_error(invocation, NOTIFICATION_BUS_NAME ".InvalidId", "Invalid notification identifier");
        return;
    }

    notification = g_hash_table_lookup(context->ids, GUINT_TO_POINTER(id));
    if ( notification != NULL )
    {
        EventdEvent *event;
        event = eventd_event_new(".notification", "close");
        eventd_event_add_data_string(event, g_strdup("source-event"), g_strdup(eventd_event_get_uuid(notification->event)));
        eventd_plugin_core_push_event(context->core, event);
        eventd_event_unref(event);
    }

    g_dbus_method_invocation_return_value(invocation, NULL);
}

static void
_eventd_fdo_notifications_get_capabilities(EventdPluginContext *context, GDBusMethodInvocation *invocation)
{
    g_dbus_method_invocation_return_value(invocation, g_variant_ref(context->capabilities));
}

static void
_eventd_fdo_notifications_get_server_information(EventdPluginContext *context, GDBusMethodInvocation *invocation)
{
    g_dbus_method_invocation_return_value(invocation, g_variant_ref(context->server_information));
}

/* D-Bus method callback */
static void
_eventd_fdo_notifications_method(GDBusConnection       *connection,
                    const char            *sender,
                    const char            *object_path,
                    const char            *interface_name,
                    const char            *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
    EventdPluginContext *context = user_data;

    /*
     * We check the method name and call our corresponding function
     */

    if ( g_strcmp0(method_name, "Notify") == 0 )
        _eventd_fdo_notifications_notify(context, sender, parameters, invocation);
    else if ( g_strcmp0(method_name, "CloseNotification") == 0 )
        _eventd_fdo_notifications_close_notification(context, parameters, invocation);
    else if ( g_strcmp0(method_name, "GetCapabilities") == 0 )
        _eventd_fdo_notifications_get_capabilities(context, invocation);
    else if ( g_strcmp0(method_name, "GetServerInformation") == 0 )
        _eventd_fdo_notifications_get_server_information(context, invocation);
}


/*
 * D-Bus interface information
 */

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
    .method_call = _eventd_fdo_notifications_method
};


/*
 * D-Bus bus callbacks
 */

static void
_eventd_fdo_notifications_on_bus_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    EventdPluginContext *context = user_data;
    GError *error = NULL;

    context->connection = connection;

    context->object_id = g_dbus_connection_register_object(connection, NOTIFICATION_BUS_PATH, context->introspection_data->interfaces[0], &interface_vtable, context, NULL, &error);
    if ( context->object_id == 0 )
    {
        g_warning("Couldn't register object: %s", error->message);
        g_clear_error(&error);
    }
}

static void
_eventd_fdo_notifications_on_name_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
#ifdef EVENTD_DEBUG
    g_debug("Acquired the name %s on the session bus", name);
#endif /* EVENTD_DEBUG */
}

static void
_eventd_fdo_notifications_on_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
#ifdef EVENTD_DEBUG
    g_debug("Lost the name %s on the session bus", name);
#endif /* EVENTD_DEBUG */
}

static void
_eventd_fdo_notifications_init_capabilities(EventdPluginContext *context)
{
    GVariantBuilder *builder;

    builder = g_variant_builder_new(G_VARIANT_TYPE("as"));

    /*
     * We consider to support everything as we could
     * forward to
     */
    g_variant_builder_add(builder, "s", "body");
    g_variant_builder_add(builder, "s", "body-markup");
    g_variant_builder_add(builder, "s", "body-hyperlinks");
    g_variant_builder_add(builder, "s", "icon-static");
    g_variant_builder_add(builder, "s", "icon-multi");
    g_variant_builder_add(builder, "s", "image/svg+xml");
    g_variant_builder_add(builder, "s", "x-eventd-overlay-icon");
    g_variant_builder_add(builder, "s", "sound");
    g_variant_builder_add(builder, "s", "actions");
    g_variant_builder_add(builder, "s", "action-icons");
    g_variant_builder_add(builder, "s", "persistence");

    context->capabilities = g_variant_new("(as)", builder);
    g_variant_builder_unref(builder);
}

/*
 * Initialization interface
 */

static EventdPluginContext *
_eventd_fdo_notifications_init(EventdPluginCoreContext *core)
{
    EventdPluginContext *context;
    GError *error = NULL;
    GDBusNodeInfo *introspection_data = NULL;
    GRegex *regex_amp = NULL;
    GRegex *regex_markup = NULL;

    introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, &error);
    if ( introspection_data == NULL )
    {
        g_warning("Couldn't generate introspection data: %s", error->message);
        goto error;
    }

    regex_amp = g_regex_new("&(?!amp;|quot;|apos;|lt;|gt;)", G_REGEX_OPTIMIZE, 0, &error);
    if ( regex_amp == NULL )
    {
        g_warning("Couldn't create amp regex: %s", error->message);
        goto error;
    }

    regex_markup = g_regex_new("<(?!(/?[biua]>|a href=\"|img src=\"))", G_REGEX_OPTIMIZE, 0, &error);
    if ( regex_markup == NULL )
    {
        g_warning("Couldn't create markup regex: %s", error->message);
        goto error;
    }

    context = g_new0(EventdPluginContext, 1);

    context->introspection_data = introspection_data;

    context->core = core;

    context->server_information = g_variant_new("(ssss)", PACKAGE_NAME, "Quentin 'Sardem FF7' Glidic", EVENTD_VERSION, NOTIFICATION_SPEC_VERSION);
    _eventd_fdo_notifications_init_capabilities(context);

    context->regex_amp = regex_amp;
    context->regex_markup = regex_markup;

    context->notifications = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, _eventd_fdo_notifications_notification_free);
    context->ids = g_hash_table_new(g_direct_hash, g_direct_equal);

    return context;

error:
    if ( regex_markup != NULL )
        g_regex_unref(regex_markup);
    if ( regex_amp != NULL )
        g_regex_unref(regex_amp);
    if ( introspection_data != NULL )
        g_dbus_node_info_unref(introspection_data);

    g_clear_error(&error);
    return NULL;
}

static void
_eventd_fdo_notifications_uninit(EventdPluginContext *context)
{
    g_hash_table_unref(context->ids);
    g_hash_table_unref(context->notifications);

    g_regex_unref(context->regex_markup);
    g_regex_unref(context->regex_amp);

    g_variant_unref(context->capabilities);
    g_variant_unref(context->server_information);

    g_dbus_node_info_unref(context->introspection_data);

    g_free(context);
}


/*
 * Start/Stop interface
 */

static void
_eventd_fdo_notifications_start(EventdPluginContext *context)
{
    context->id = g_bus_own_name(G_BUS_TYPE_SESSION, NOTIFICATION_BUS_NAME, G_BUS_NAME_OWNER_FLAGS_NONE, _eventd_fdo_notifications_on_bus_acquired, _eventd_fdo_notifications_on_name_acquired, _eventd_fdo_notifications_on_name_lost, context, NULL);
}

static void
_eventd_fdo_notifications_stop(EventdPluginContext *context)
{
    if ( context->object_id > 0 )
        g_dbus_connection_unregister_object(context->connection, context->object_id);
    context->object_id = 0;
    g_bus_unown_name(context->id);
}


/*
 * Event dispatching interface
 */

static void
_eventd_fdo_notifications_event_dispatch(EventdPluginContext *context, EventdEvent *event)
{
    const gchar *category;
    category = eventd_event_get_category(event);
    if ( g_strcmp0(category, ".notification") != 0 )
        return;

    const gchar *uuid;
    uuid = eventd_event_get_data_string(event, "source-event");
    if ( ( uuid == NULL ) || ( ! g_hash_table_contains(context->notifications, uuid) ) )
        return;

    EventdDbusNotification *notification;
    notification = g_hash_table_lookup(context->notifications, uuid);

    const gchar *name;
    name = eventd_event_get_name(event);

    if ( g_strcmp0(name, "timeout") == 0 )
        _eventd_fdo_notifications_notification_closed(notification, EVENTD_FDO_NOTIFICATIONS_CLOSE_REASON_EXPIRED);
    else if ( g_strcmp0(name, "dismiss") == 0 )
        _eventd_fdo_notifications_notification_closed(notification, EVENTD_FDO_NOTIFICATIONS_CLOSE_REASON_DISMISS);
    else if ( g_strcmp0(name, "close") == 0 )
        _eventd_fdo_notifications_notification_closed(notification, EVENTD_FDO_NOTIFICATIONS_CLOSE_REASON_CALL);
    else
        _eventd_fdo_notifications_notification_closed(notification, EVENTD_FDO_NOTIFICATIONS_CLOSE_REASON_RESERVED);
}


/*
 * Plugin interface
 */

EVENTD_EXPORT const gchar *eventd_plugin_id = "fdo-notifications";
EVENTD_EXPORT
void
eventd_plugin_get_interface(EventdPluginInterface *interface)
{
    eventd_plugin_interface_add_init_callback(interface, _eventd_fdo_notifications_init);
    eventd_plugin_interface_add_uninit_callback(interface, _eventd_fdo_notifications_uninit);

    eventd_plugin_interface_add_start_callback(interface, _eventd_fdo_notifications_start);
    eventd_plugin_interface_add_stop_callback(interface, _eventd_fdo_notifications_stop);

    eventd_plugin_interface_add_event_dispatch_callback(interface, _eventd_fdo_notifications_event_dispatch);
}
