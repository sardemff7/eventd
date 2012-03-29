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

#include <string.h>
#include <glib.h>
#include <gio/gio.h>

#include <libeventd-event.h>
#include <eventd-core-interface.h>
#include <eventd-plugin.h>

#define NOTIFICATION_BUS_NAME      "org.freedesktop.Notifications"
#define NOTIFICATION_BUS_PATH      "/org/freedesktop/Notifications"

#define NOTIFICATION_SPEC_VERSION  "1.2"

struct _EventdPluginContext {
    EventdCoreContext *core;
    EventdCoreInterface *core_interface;
    GDBusNodeInfo *introspection_data;
    gboolean disabled;
    guint id;
    GDBusConnection *connection;
    guint32 count;
    GHashTable *events;
    GHashTable *notifications;
};

typedef struct {
    EventdPluginContext *context;
    guint32 id;
    gchar *sender;
    EventdEvent *event;
} EventdDbusNotification;

static void
_eventd_dbus_event_ended(EventdEvent *event, EventdEventEndReason reason, EventdDbusNotification *notification)
{
    g_dbus_connection_emit_signal(notification->context->connection, notification->sender,
                                  NOTIFICATION_BUS_PATH, NOTIFICATION_BUS_NAME,
                                  "NotificationClosed", g_variant_new("(uu)", notification->id, reason),
                                  NULL);
    g_hash_table_remove(notification->context->notifications, GUINT_TO_POINTER(notification->id));
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
_eventd_dbus_notification_new(EventdPluginContext *context, const gchar *sender, guint32 id, EventdEvent *event)
{
    EventdDbusNotification *notification;

    notification = g_new0(EventdDbusNotification, 1);
    notification->context = context;
    notification->id = id;
    notification->sender = g_strdup(sender);
    notification->event = event;

    g_hash_table_insert(context->notifications, GUINT_TO_POINTER(notification->id), notification);
    g_signal_connect(event, "ended", G_CALLBACK(_eventd_dbus_event_ended), notification);
}

static void
_eventd_dbus_notify(EventdPluginContext *context, const gchar *sender, GVariant *parameters, GDBusMethodInvocation *invocation)
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
    GVariant *image_data = NULL;
    const gchar *image_path = NULL;

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
            image_data = g_variant_ref(hint);
        else if ( ( g_strcmp0(hint_name, "image-path") == 0 )
                  || ( g_strcmp0(hint_name, "image_path") == 0 ) )
            image_path = g_variant_get_string(hint, NULL);
        else if ( ( g_strcmp0(hint_name, "icon_data") == 0 )
                  && image_data == NULL )
            image_data = g_variant_ref(hint);
        else if ( g_strcmp0(hint_name, "sound-file") == 0 )
            {}

        g_variant_unref(hint);
    }

#if DEBUG
    g_debug("Creanting event '%s' for client '%s' ", event_name, app_name);
#endif /* DEBUG */

    if ( id > 0 )
    {
        /*
         * TODO: Update the notification
         */
    }
    else
        id = ++context->count;
    event = eventd_event_new(event_name);
    eventd_event_set_category(event, "libnotify");

    eventd_event_add_data(event, g_strdup("client-name"), g_strdup(app_name));

    eventd_event_add_data(event, g_strdup("summary"), g_strdup(summary));

    if ( body != NULL )
        eventd_event_add_data(event, g_strdup("body"), g_strdup(body));

    if ( ( icon != NULL ) && ( *icon != 0 ) )
    {
#if DEBUG
        g_debug("Icon specified: '%s'", icon);
#endif /* DEBUG */

        if ( g_str_has_prefix(icon, "file://") )
            eventd_event_add_data(event, g_strdup("icon"), g_strdup(icon));
        else
        {
            /*
             * TODO: Support freedesktop themes
             */
        }
    }

    if ( image_data != NULL )
    {
        gboolean a;
        gint b, w, h, s, n;
        GVariant *data;
        gchar *format;

        g_variant_get(image_data, "(iiibii@ay)", &w, &h, &s, &a, &b, &n, &data);
        eventd_event_add_data(event, g_strdup("image"), g_base64_encode(g_variant_get_data(data), g_variant_get_size(data)));

        format = g_strdup_printf("%x %x %x %x", w, h, s, a);
        eventd_event_add_data(event, g_strdup("image-format"), format);
    }
    else if ( image_path != NULL )
    {
        if ( g_str_has_prefix(image_path, "file://") )
            eventd_event_add_data(event, g_strdup("image"), g_strdup(image_path));
    }


    eventd_event_set_timeout(event, ( timeout > -1 ) ? timeout : ( urgency > -1 ) ? ( 3000 + urgency * 2000 ) : -1);

    _eventd_dbus_notification_new(context, sender, id, event);
    context->core_interface->push_event(context->core, event);

    g_dbus_method_invocation_return_value(invocation, g_variant_new("(u)", id));
}

static void
_eventd_dbus_close_notification(EventdPluginContext *context, GVariant *parameters, GDBusMethodInvocation *invocation)
{
    guint32 id;
    EventdEvent *event;

    g_variant_get(parameters, "(u)", &id);

    if ( id == 0 )
    {
        g_dbus_method_invocation_return_dbus_error(invocation, NOTIFICATION_BUS_NAME ".InvalidId", "Invalid notification identifier");
        return;
    }

    event = g_hash_table_lookup(context->notifications, GUINT_TO_POINTER(id));

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
    g_variant_builder_add(builder, "s", "icon-static");
    g_variant_builder_add(builder, "s", "image/svg+xml");

    /* eventd special features */
    g_variant_builder_add(builder, "s", "x-eventd-overlay-icon");
    g_variant_builder_add(builder, "s", "x-eventd-user-control");

    /* TODO: make the notification plugin works fine with these
    g_variant_builder_add(builder, "s", "body-hyperlinks");
    */

    /* TODO: pass the data to the sound plugin
    g_variant_builder_add(builder, "s", "sound");
    */

    /* TODO: or not, these imply “some” work
    g_variant_builder_add(builder, "s", "body-images");
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
    EventdPluginContext *context = user_data;

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
    EventdPluginContext *context = user_data;
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
#if DEBUG
    g_debug("Acquired the name %s on the session bus", name);
#endif /* DEBUG */
}

static void
_eventd_dbus_on_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
#if DEBUG
    g_debug("Lost the name %s on the session bus", name);
#endif /* DEBUG */
}

static EventdPluginContext *
_eventd_dbus_init(EventdCoreContext *core, EventdCoreInterface *core_interface)
{
    EventdPluginContext *context;
    GError *error = NULL;
    GDBusNodeInfo *introspection_data;

    introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, &error);
    if ( introspection_data == NULL )
    {
        g_warning("Couldn’t generate introspection data: %s", error->message);
        g_clear_error(&error);
        return NULL;
    }

    context = g_new0(EventdPluginContext, 1);

    context->introspection_data = introspection_data;

    context->core = core;
    context->core_interface = core_interface;

    context->notifications = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, _eventd_dbus_notification_free);

    return context;
}

static void
_eventd_dbus_start(EventdPluginContext *context)
{
    if ( ( context == NULL ) || context->disabled )
        return;

    context->id = g_bus_own_name(G_BUS_TYPE_SESSION, NOTIFICATION_BUS_NAME, G_BUS_NAME_OWNER_FLAGS_NONE, _eventd_dbus_on_bus_acquired, _eventd_dbus_on_name_acquired, _eventd_dbus_on_name_lost, context, NULL);
}

static void
_eventd_dbus_stop(EventdPluginContext *context)
{
    if ( ( context == NULL ) || context->disabled )
        return;

    g_bus_unown_name(context->id);
}

static void
_eventd_dbus_uninit(EventdPluginContext *context)
{
    if ( context == NULL )
        return;

    g_hash_table_unref(context->notifications);

    g_free(context);
}

static GOptionGroup *
_eventd_dbus_get_option_group(EventdPluginContext *context)
{
    GOptionGroup *option_group;
    GOptionEntry entries[] =
    {
        { "no-dbus", 'D', 0, G_OPTION_ARG_NONE, &context->disabled, "Disable D-Bus interface", NULL },
        { NULL }
    };

    option_group = g_option_group_new("dbus", "D-Bus plugin options", "Show D-Bus plugin help options", NULL, NULL);
    g_option_group_set_translation_domain(option_group, GETTEXT_PACKAGE);
    g_option_group_add_entries(option_group, entries);
    return option_group;
}

const gchar *eventd_plugin_id = "eventd-dbus";
void
eventd_plugin_get_info(EventdPlugin *plugin)
{
    plugin->init = _eventd_dbus_init;
    plugin->uninit = _eventd_dbus_uninit;

    plugin->get_option_group = _eventd_dbus_get_option_group;

    plugin->start = _eventd_dbus_start;
    plugin->stop = _eventd_dbus_stop;
}
