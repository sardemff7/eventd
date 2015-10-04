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
#include <gio/gio.h>

#include <libeventd-event.h>
#include <eventd-plugin.h>

#define NOTIFICATION_BUS_NAME      "org.freedesktop.Notifications"
#define NOTIFICATION_BUS_PATH      "/org/freedesktop/Notifications"

#define NOTIFICATION_SPEC_VERSION  "1.2"

struct _EventdPluginContext {
    EventdPluginCoreContext *core;
    EventdPluginCoreInterface *core_interface;
    GDBusNodeInfo *introspection_data;
    guint id;
    GDBusConnection *connection;
    GVariant *capabilities;
    GVariant *server_information;
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
_eventd_fdo_notifications_event_ended(EventdEvent *event, EventdEventEndReason reason, EventdDbusNotification *notification)
{
    /*
     * We have to emit the NotificationClosed signal for our D-Bus client
     */

    g_dbus_connection_emit_signal(notification->context->connection, notification->sender,
                                  NOTIFICATION_BUS_PATH, NOTIFICATION_BUS_NAME,
                                  "NotificationClosed", g_variant_new("(uu)", notification->id, reason),
                                  NULL);
    g_hash_table_remove(notification->context->notifications, GUINT_TO_POINTER(notification->id));
}


/*
 * Helper functions
 */

static guint32
_eventd_fdo_notifications_notification_new(EventdPluginContext *context, const gchar *sender, EventdEvent *event)
{
    if ( ! eventd_plugin_core_push_event(context->core, context->core_interface, event) )
        return 0;

    EventdDbusNotification *notification;

    notification = g_new0(EventdDbusNotification, 1);
    notification->context = context;
    notification->id = ++context->count;
    notification->sender = g_strdup(sender);
    notification->event = event;

    eventd_event_add_data(event, g_strdup("id"), g_strdup_printf("%u", notification->id));

    g_hash_table_insert(context->notifications, GUINT_TO_POINTER(notification->id), notification);
    g_signal_connect(event, "ended", G_CALLBACK(_eventd_fdo_notifications_event_ended), notification);

    return notification->id;
}

static void
_eventd_fdo_notifications_notification_free(gpointer user_data)
{
    EventdDbusNotification *notification = user_data;

    g_object_unref(notification->event);

    g_free(notification->sender);

    g_free(notification);
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
    const gchar *event_name = "libnotify";
    gint16 urgency = -1;
    GVariant *image_data = NULL;
    const gchar *image_path = NULL;
    const gchar *sound_name = NULL;
    const gchar *sound_file = NULL;
    gboolean no_sound = FALSE;

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

#ifdef EVENTD_DEBUG
    g_debug("Received notification from '%s': '%s'", app_name, summary);
#endif /* EVENTD_DEBUG */

    while ( g_variant_iter_next(hints, "{&sv}", &hint_name, &hint) )
    {
#ifdef EVENTD_DEBUG
        g_debug("Found hint '%s'", hint_name);
#endif /* EVENTD_DEBUG */

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
        else if ( g_strcmp0(hint_name, "sound-name") == 0 )
            sound_name = g_variant_get_string(hint, NULL);
        else if ( g_strcmp0(hint_name, "sound-file") == 0 )
            sound_file = g_variant_get_string(hint, NULL);
        else if ( g_strcmp0(hint_name, "suppress-sound") == 0 )
            no_sound = g_variant_get_boolean(hint);

        g_variant_unref(hint);
    }

#ifdef EVENTD_DEBUG
    g_debug("Creating event '%s' for client '%s' ", event_name, app_name);
#endif /* EVENTD_DEBUG */

    if ( id > 0 )
    {
        EventdDbusNotification *notification;

        notification = g_hash_table_lookup(context->notifications, GUINT_TO_POINTER(id));
        if ( notification == NULL )
        {
            g_dbus_method_invocation_return_dbus_error(invocation, NOTIFICATION_BUS_NAME ".InvalidId", "Invalid notification identifier");
            return;
        }
        event = notification->event;
    }
    else
        event = eventd_event_new("libnotify", event_name);

    eventd_event_add_data(event, g_strdup("client-name"), g_strdup(app_name));

    eventd_event_add_data(event, g_strdup("summary"), g_strdup(summary));

    if ( body != NULL )
        eventd_event_add_data(event, g_strdup("body"), g_strdup(body));

    if ( ( icon != NULL ) && ( *icon != 0 ) )
    {
#ifdef EVENTD_DEBUG
        g_debug("Icon specified: '%s'", icon);
#endif /* EVENTD_DEBUG */

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

    if ( ! no_sound )
    {
        if ( sound_name != NULL )
            eventd_event_add_data(event, g_strdup("sound-name"), g_strdup(sound_name));

        if ( sound_file != NULL )
            eventd_event_add_data(event, g_strdup("sound-file"), g_strdup_printf("file://%s", sound_file));
    }


    eventd_event_set_timeout(event, ( timeout > -1 ) ? timeout : ( urgency > -1 ) ? ( 3000 + urgency * 2000 ) : -1);

    if ( id > 0 )
        eventd_event_update(event, NULL);
    else
        id = _eventd_fdo_notifications_notification_new(context, sender, event);

    if ( id == 0 )
    {
        g_object_unref(event);
        g_dbus_method_invocation_return_dbus_error(invocation, NOTIFICATION_BUS_NAME ".InvalidNotification", "Invalid notification type");
        return;
    }

    g_dbus_method_invocation_return_value(invocation, g_variant_new("(u)", id));
}

static void
_eventd_fdo_notifications_close_notification(EventdPluginContext *context, GVariant *parameters, GDBusMethodInvocation *invocation)
{
    guint32 id;
    EventdDbusNotification *notification;

    g_variant_get(parameters, "(u)", &id);

    if ( id == 0 )
    {
        g_dbus_method_invocation_return_dbus_error(invocation, NOTIFICATION_BUS_NAME ".InvalidId", "Invalid notification identifier");
        return;
    }

    notification = g_hash_table_lookup(context->notifications, GUINT_TO_POINTER(id));

    if ( notification != NULL )
        eventd_event_end(notification->event, EVENTD_EVENT_END_REASON_CLIENT_DISMISS);

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
    guint object_id;

    context->connection = connection;

    object_id = g_dbus_connection_register_object(connection, NOTIFICATION_BUS_PATH, context->introspection_data->interfaces[0], &interface_vtable, context, NULL, &error);
    if ( object_id == 0 )
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
_eventd_fdo_notifications_load_capabilities_dir(const gchar *dir_name, GHashTable *capabilities_set)
{
    if ( ! g_file_test(dir_name, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR) )
        return;

    GDir *capabilities_dir;
    GError *error = NULL;
    capabilities_dir = g_dir_open(dir_name, 0, &error);
    if ( capabilities_dir == NULL )
    {
        g_warning("Couldn't read the Freedesktop.org Notifications plugin capabilities directory '%s': %s", dir_name, error->message);
        g_clear_error(&error);
        return;
    }

    const gchar *file;
    while ( ( file = g_dir_read_name(capabilities_dir) ) != NULL )
    {
        if ( g_str_has_prefix(file, "." ) || ( ! g_str_has_suffix(file, ".capabilities") ) )
            continue;

        gchar *full_filename;

        full_filename = g_build_filename(dir_name, file, NULL);
        if ( ! g_file_test(full_filename, G_FILE_TEST_IS_REGULAR) )
            goto next;

        gchar *capabilities;
        if ( ! g_file_get_contents(full_filename, &capabilities, NULL, &error) )
        {
            g_warning("Could not read capability file '%s': %s", file, error->message);
            g_clear_error(&error);
            goto next;
        }

        gchar **capabilitiesv;
        capabilitiesv = g_strsplit_set(capabilities, " \n,", -1);
        g_free(capabilities);

        gchar **capability;
        for ( capability = capabilitiesv ; *capability != NULL ; ++capability )
            g_hash_table_insert(capabilities_set, *capability, NULL);
        g_free(capabilitiesv);

    next:
        g_free(full_filename);
    }
    g_dir_close(capabilities_dir);
}

static void
_eventd_fdo_notifications_init_capabilities(EventdPluginContext *context)
{
    /*
     * We use a GHashTable to make sure we
     * add each capability only once
     */
    GHashTable *capabilities_set;

    capabilities_set = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    /* Common capabilities */
    /* TODO: or not, these imply “some” work
    g_hash_table_insert(capabilities_set, "actions", NULL);
     */

    _eventd_fdo_notifications_load_capabilities_dir(FDONOTIFICATIONSCAPABILITIESDIR, capabilities_set);

    GVariantBuilder *builder;

    builder = g_variant_builder_new(G_VARIANT_TYPE("as"));

    GHashTableIter iter;
    const gchar *capability;
    gpointer dummy;
    g_hash_table_iter_init(&iter, capabilities_set);
    while ( g_hash_table_iter_next(&iter, (gpointer *)&capability, &dummy) )
    {
        if ( *capability != '\0' )
            /* Do not add empty capabilities */
            g_variant_builder_add(builder, "s", capability);
    }

    g_hash_table_unref(capabilities_set);

    context->capabilities = g_variant_new("(as)", builder);
    g_variant_builder_unref(builder);
}

/*
 * Initialization interface
 */

static EventdPluginContext *
_eventd_fdo_notifications_init(EventdPluginCoreContext *core, EventdPluginCoreInterface *core_interface)
{
    EventdPluginContext *context;
    GError *error = NULL;
    GDBusNodeInfo *introspection_data;

    introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, &error);
    if ( introspection_data == NULL )
    {
        g_warning("Couldn't generate introspection data: %s", error->message);
        g_clear_error(&error);
        return NULL;
    }

    context = g_new0(EventdPluginContext, 1);

    context->introspection_data = introspection_data;

    context->core = core;
    context->core_interface = core_interface;

    context->server_information = g_variant_new("(ssss)", PACKAGE_NAME, "Quentin 'Sardem FF7' Glidic", PACKAGE_VERSION, NOTIFICATION_SPEC_VERSION);
    _eventd_fdo_notifications_init_capabilities(context);

    context->notifications = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, _eventd_fdo_notifications_notification_free);

    return context;
}

static void
_eventd_fdo_notifications_uninit(EventdPluginContext *context)
{
    g_hash_table_unref(context->notifications);

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
    g_bus_unown_name(context->id);
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
}
