/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2015 Quentin "Sardem FF7" Glidic
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
#include <glib-object.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libeventd-event.h>
#include <eventd-plugin.h>
#include <libeventd-helpers-config.h>

/* We share nd plugin’s pixbuf loader */
#include "plugins/nd/src/pixbuf.h"

/*
 * D-Bus interface information
 */

#define NOTIFICATION_BUS_NAME      "org.freedesktop.Notifications"
#define NOTIFICATION_BUS_PATH      "/org/freedesktop/Notifications"

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

typedef enum {
    EVENTD_LIBNOTIFY_SPEC_VERSION_OLD,
    EVENTD_LIBNOTIFY_SPEC_VERSION_1_1,
    EVENTD_LIBNOTIFY_SPEC_VERSION_1_2,
} EventdLibnotifySpecVersion;

typedef enum {
    EVENTD_LIBNOTIFY_URGENCY_LOW,
    EVENTD_LIBNOTIFY_URGENCY_NORMAL,
    EVENTD_LIBNOTIFY_URGENCY_CRITICAL,
    _EVENTD_LIBNOTIFY_URGENCY_SIZE
} EventdLibnotifyUrgency;

struct _EventdPluginContext
{
    GDBusNodeInfo *introspection_data;
    guint id;
    gboolean bus_name_owned;
    GDBusProxy *server;
    GSList *actions;
    struct {
        EventdLibnotifySpecVersion spec_version;
    } information;
    struct {
        gboolean overlay_icon;
        gboolean svg_support;
    } capabilities;
};

struct _EventdPluginAction {
    FormatString *title;
    FormatString *message;
    Filename *image;
    Filename *icon;
    gdouble scale;
    EventdLibnotifyUrgency urgency;
    GHashTable *hints;
};

typedef struct {
    EventdPluginContext *context;
    guint32 id;
    EventdEvent *event;
} EventdLibnotifyNotification;

const gchar * const _eventd_libnotify_urgency[_EVENTD_LIBNOTIFY_URGENCY_SIZE] = {
    [EVENTD_LIBNOTIFY_URGENCY_LOW] =      "low",
    [EVENTD_LIBNOTIFY_URGENCY_NORMAL] =   "normal",
    [EVENTD_LIBNOTIFY_URGENCY_CRITICAL] = "critical",
};

static GdkPixbuf *
_eventd_libnotify_get_image(EventdPluginContext *context, EventdPluginAction *action, EventdEvent *event, gchar **icon_uri, gchar **image_uri)
{
    gchar *uri;
    GdkPixbuf *image = NULL;
    GdkPixbuf *icon = NULL;

    uri = evhelpers_filename_get_uri(action->image, event, "icons");
    if ( uri != NULL )
    {
        if ( g_str_has_prefix(uri, "file://")
                /* Check that the server supports at least image_path hint */
                && ( context->information.spec_version >= EVENTD_LIBNOTIFY_SPEC_VERSION_1_1 )
                /* Check for SVG support */
                && ( context->capabilities.svg_support || ( ! g_str_has_suffix(uri, ".svg") ) )
            )
            *image_uri = uri;
        else
            image = eventd_nd_pixbuf_from_uri(uri, 0, 0);
    }

    uri = evhelpers_filename_get_uri(action->icon, event, "icons");
    if ( uri != NULL )
    {
        if ( g_str_has_prefix(uri, "file://")
                /* Check for SVG support */
                && ( context->capabilities.svg_support || ( ! g_str_has_suffix(uri, ".svg") ) )
            )
            *icon_uri = uri;
        else
            icon = eventd_nd_pixbuf_from_uri(uri, 0, 0);
    }

    if ( ( *image_uri == NULL ) && ( image == NULL ) )
        return icon;

    if ( ( *icon_uri == NULL ) && ( icon == NULL ) )
        return image;

    /*
     * The server can support both icon and image
     * We have an icon URI and an image (either URI or data)
     * We give it both
     */
    if ( context->capabilities.overlay_icon && ( *icon_uri != NULL ) && ( ( *image_uri != NULL ) || ( image != NULL ) ) )
        return image;

    /*
     * We have an image URI, check if we can read it
     */
    if ( ( image == NULL ) && ( *image_uri != NULL ) )
    {
        image = eventd_nd_pixbuf_from_uri(*image_uri, 0, 0);
        *image_uri = NULL;
    }

    /*
     * We could not read the image, so back to sending the icon alone
     */
    if ( image == NULL )
        return icon;

    /*
     * We have an icon URI, check if we can read it
     */
    if ( ( icon == NULL ) && ( *icon_uri != NULL ) )
    {
        icon = eventd_nd_pixbuf_from_uri(*icon_uri, 0, 0);
        *icon_uri = NULL;
    }

    /*
     * We could not read the icon, so back to sending the image alone
     */
    if ( icon == NULL )
        return image;

    /*
     * Either the server does not support displaying both
     * or we only had data for the icon and no URI
     * Here we are, merge then!
     */
    gint image_width, image_height;
    gint icon_width, icon_height;
    gint x, y;
    gdouble scale;

    image_width = gdk_pixbuf_get_width(image);
    image_height = gdk_pixbuf_get_height(image);

    icon_width = action->scale * (gdouble) image_width;
    icon_height = action->scale * (gdouble) image_height;

    x = image_width - icon_width;
    y = image_height - icon_height;

    scale = (gdouble) icon_width / (gdouble) gdk_pixbuf_get_width(icon);

    gdk_pixbuf_composite(icon, image,
                         x, y,
                         icon_width, icon_height,
                         x, y,
                         scale, scale,
                         GDK_INTERP_BILINEAR, 255);

    g_object_unref(icon);

    return image;
}

/*
 * Event contents helper
 */

static EventdPluginAction *
_eventd_libnotify_event_new(FormatString *title, FormatString *message, Filename *image, Filename *icon, gint64 scale, EventdLibnotifyUrgency urgency, GHashTable *hints)
{
    EventdPluginAction *event;

    event = g_slice_new0(EventdPluginAction);

    event->title = title;
    event->message = message;
    event->image = image;
    event->icon = icon;
    event->scale = (gdouble) scale / 100.;
    event->urgency = urgency;
    event->hints = hints;

    return event;
}

static void
_eventd_libnotify_action_free(gpointer data)
{
    EventdPluginAction *event = data;

    if ( event->hints != NULL )
        g_hash_table_unref(event->hints);
    evhelpers_filename_unref(event->image);
    evhelpers_filename_unref(event->icon);
    evhelpers_format_string_unref(event->message);
    evhelpers_format_string_unref(event->title);

    g_slice_free(EventdPluginAction, event);
}

static void
_eventd_libnotify_notification_free(gpointer data)
{
    EventdLibnotifyNotification *self = data;

    g_object_unref(self->event);

    g_slice_free(EventdLibnotifyNotification, self);
}

static void
_eventd_libnotify_notification_close(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    EventdLibnotifyNotification *self = user_data;
    GVariant *ret;
    GError *error = NULL;

    ret = g_dbus_proxy_call_finish(self->context->server, res, &error);
    if ( ret == NULL )
    {
        g_warning("Couldn't close the notification: %s", error->message);
        g_clear_error(&error);
    }
    else
        g_variant_unref(ret);
    _eventd_libnotify_notification_free(self);
}

static void
_eventd_libnotify_event_ended(EventdLibnotifyNotification *self, EventdEventEndReason reason, EventdEvent *event)
{
    if ( self->id > 0 )
        g_dbus_proxy_call(self->context->server, "CloseNotification", g_variant_new("(u)", self->id), G_DBUS_CALL_FLAGS_NONE, -1, NULL, _eventd_libnotify_notification_close, self);
    else
        _eventd_libnotify_notification_free(self);
}


/*
 * Init interface
 */

static EventdPluginContext *
_eventd_libnotify_init(EventdPluginCoreContext *core, EventdPluginCoreInterface *core_interface)
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

    return context;
}

static void
_eventd_libnotify_uninit(EventdPluginContext *context)
{
    g_dbus_node_info_unref(context->introspection_data);

    g_free(context);
}


/*
 * Start/stop interface
 */

static void
_eventd_libnotify_proxy_get_server_information(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    EventdPluginContext *context = user_data;
    GError *error = NULL;
    GVariant *ret;

    ret = g_dbus_proxy_call_finish(context->server, res, &error);
    if ( ret == NULL )
    {
        g_warning("Couldn't get org.freedesktop.Notifications server information: %s", error->message);
        g_clear_error(&error);
        return;
    }

    const gchar *spec_version;
    g_variant_get(ret, "(&s&s&s&s)", NULL, NULL, NULL, &spec_version);

    if ( g_strcmp0(spec_version, "1.2") == 0 )
        context->information.spec_version = EVENTD_LIBNOTIFY_SPEC_VERSION_1_2;
    else if ( g_strcmp0(spec_version, "1.1") == 0 )
        context->information.spec_version = EVENTD_LIBNOTIFY_SPEC_VERSION_1_1;
    else
        context->information.spec_version = EVENTD_LIBNOTIFY_SPEC_VERSION_OLD;

    g_variant_unref(ret);
}

static void
_eventd_libnotify_proxy_get_capabilities(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    EventdPluginContext *context = user_data;
    GError *error = NULL;
    GVariant *ret;

    ret = g_dbus_proxy_call_finish(context->server, res, &error);
    if ( ret == NULL )
    {
        g_warning("Couldn't get org.freedesktop.Notifications server capabilities: %s", error->message);
        g_clear_error(&error);
        return;
    }

    const gchar **capabilities, **capability;
    g_variant_get(ret, "(^a&s)", &capabilities);

    for ( capability = capabilities ; *capability != NULL ; ++capability )
    {
        if ( g_strcmp0(*capability, "x-eventd-overlay-icon") == 0 )
            context->capabilities.overlay_icon = TRUE;
        else if ( g_strcmp0(*capability, "image/svg+xml") == 0 )
            context->capabilities.svg_support = TRUE;
    }

    g_free(capabilities);
    g_variant_unref(ret);
}

static void
_eventd_libnotify_proxy_create_callback(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    EventdPluginContext *context = user_data;
    GError *error = NULL;

    context->server = g_dbus_proxy_new_finish(res, &error);
    if ( context->server == NULL )
    {
        g_warning("Couldn't connection to org.freedesktop.Notifications server: %s", error->message);
        g_clear_error(&error);
        return;
    }

    context->capabilities.overlay_icon = FALSE;
    context->capabilities.svg_support = FALSE;

    g_dbus_proxy_call(context->server, "GetServerInformation", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, _eventd_libnotify_proxy_get_server_information, context);
    g_dbus_proxy_call(context->server, "GetCapabilities", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, _eventd_libnotify_proxy_get_capabilities, context);
}

static void
_eventd_libnotify_bus_name_appeared(GDBusConnection *connection, const gchar *name, const gchar *name_owner, gpointer user_data)
{
    EventdPluginContext *context = user_data;
    GDBusProxyFlags flags = G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS|G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES;

    if ( g_strcmp0(g_dbus_connection_get_unique_name(connection), name_owner) == 0 )
    {
        /* The fdo-notifications plugin got the bus name */
        context->bus_name_owned = TRUE;
        return;
    }
    g_dbus_proxy_new(connection, flags, context->introspection_data->interfaces[0], name, NOTIFICATION_BUS_PATH, NOTIFICATION_BUS_NAME, NULL, _eventd_libnotify_proxy_create_callback, context);
}

static void
_eventd_libnotify_bus_name_vanished(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    EventdPluginContext *context = user_data;
    context->bus_name_owned = FALSE;
    if ( context->server != NULL )
        g_object_unref(context->server);
}

static void
_eventd_libnotify_start(EventdPluginContext *context)
{
    context->id = g_bus_watch_name(G_BUS_TYPE_SESSION, NOTIFICATION_BUS_NAME, G_BUS_NAME_WATCHER_FLAGS_NONE, _eventd_libnotify_bus_name_appeared, _eventd_libnotify_bus_name_vanished, context, NULL);
}

static void
_eventd_libnotify_stop(EventdPluginContext *context)
{
    g_bus_unwatch_name(context->id);
}


/*
 * Configuration interface
 */

static EventdPluginAction *
_eventd_libnotify_action_parse(EventdPluginContext *context, GKeyFile *config_file)
{
    gboolean disable;

    if ( ! g_key_file_has_group(config_file, "Libnotify") )
        return NULL;

    if ( evhelpers_config_key_file_get_boolean(config_file, "Libnotify", "Disable", &disable) < 0 )
        return NULL;

    if ( disable )
        return NULL;

    FormatString *title = NULL;
    FormatString *message = NULL;
    Filename *image = NULL;
    Filename *icon = NULL;
    gint64 scale;
    guint64 urgency;

    if ( evhelpers_config_key_file_get_locale_format_string_with_default(config_file, "Libnotify", "Title", NULL, "${summary}", &title) < 0 )
        goto skip;
    if ( evhelpers_config_key_file_get_locale_format_string_with_default(config_file, "Libnotify", "Message", NULL, "${body}", &message) < 0 )
        goto skip;
    if ( evhelpers_config_key_file_get_filename_with_default(config_file, "Libnotify", "Image", "image", &image) < 0 )
        goto skip;
    if ( evhelpers_config_key_file_get_filename_with_default(config_file, "Libnotify", "Icon", "icon", &icon) < 0 )
        goto skip;
    if ( evhelpers_config_key_file_get_int_with_default(config_file, "Libnotify", "OverlayScale", 50, &scale) < 0 )
        goto skip;
    if ( evhelpers_config_key_file_get_enum_with_default(config_file, "Libnotify", "Urgency", _eventd_libnotify_urgency, _EVENTD_LIBNOTIFY_URGENCY_SIZE, EVENTD_LIBNOTIFY_URGENCY_NORMAL, &urgency) < 0 )
        goto skip;

    if ( scale < 0 )
        scale = 50;
    scale = CLAMP(scale, 0, 100);

    GHashTable *hints = NULL;
    if ( g_key_file_has_group(config_file, "Libnotify hints") )
    {
        gchar **keys, **key;
        FormatString *value;

        keys = g_key_file_get_keys(config_file, "Libnotify hints", NULL, NULL);
        if ( keys != NULL )
        {
            hints = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) evhelpers_format_string_unref);
            for ( key = keys ; *key != NULL ; ++key )
            {
                if ( evhelpers_config_key_file_get_format_string(config_file, "Libnotify hints", *key, &value) == 0 )
                    g_hash_table_insert(hints, *key, value);
                else
                    g_free(*key);
            }
            g_free(keys);
        }
    }

    EventdPluginAction *action;
    action = _eventd_libnotify_event_new(title, message, image, icon, scale, urgency, hints);
    title = message = NULL;
    image = icon = NULL;

    context->actions = g_slist_prepend(context->actions, action);
    return action;

skip:
    evhelpers_filename_unref(icon);
    evhelpers_filename_unref(image);
    evhelpers_format_string_unref(message);
    evhelpers_format_string_unref(title);
    return NULL;
}

static void
_eventd_libnotify_config_reset(EventdPluginContext *context)
{
    g_slist_free_full(context->actions, _eventd_libnotify_action_free);
    context->actions = NULL;
}


/*
 * Event action interface
 */

static void
_eventd_libnotify_proxy_notify(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    EventdLibnotifyNotification *self = user_data;
    GVariant *ret;
    GError *error = NULL;

    ret = g_dbus_proxy_call_finish(self->context->server, res, &error);
    if ( ret == NULL )
    {
        g_warning("Server refused notification: %s", error->message);
        g_clear_error(&error);
        return;
    }

    g_variant_get(ret, "(u)", &self->id);
    g_variant_unref(ret);
}

static void
_eventd_libnotify_event_action(EventdPluginContext *context, EventdPluginAction *action, EventdEvent *event)
{
    if ( context->bus_name_owned )
        /* We would be sending stuff to ourselves */
        return;

    if ( context->server == NULL )
    {
        g_warning("No server on the bus");
        return;
    }

    gchar *title;
    gchar *message;
    gchar *icon_uri = NULL;
    gchar *image_uri = NULL;
    GdkPixbuf *image;

    title = evhelpers_format_string_get_string(action->title, event, NULL, NULL);
    message = evhelpers_format_string_get_string(action->message, event, NULL, NULL);

    image = _eventd_libnotify_get_image(context, action, event, &icon_uri, &image_uri);

    GVariantBuilder *hints;
    hints = g_variant_builder_new(G_VARIANT_TYPE_VARDICT);

    if ( action->hints != NULL )
    {
        GHashTableIter iter;
        gchar *key;
        FormatString *value_;
        gchar *value;
        g_hash_table_iter_init(&iter, action->hints);
        while ( g_hash_table_iter_next(&iter, (gpointer *) &key, (gpointer *) &value_) )
        {
            value = evhelpers_format_string_get_string(value_, event, NULL, NULL);
            if ( *value == '\0' )
                g_variant_builder_add(hints, "{sv}", key, NULL);
            else
                g_variant_builder_add_parsed(hints, "{'%s', %?}", key, value);
            g_free(value);
        }
    }

    g_variant_builder_add(hints, "{sv}", "urgency", g_variant_new_byte(action->urgency));

    const gchar *image_path_hint = NULL;
    const gchar *image_data_hint = NULL;
    switch ( context->information.spec_version )
    {
    case EVENTD_LIBNOTIFY_SPEC_VERSION_1_2:
        image_path_hint = "image-path";
        image_data_hint = "image-data";
    break;
    case EVENTD_LIBNOTIFY_SPEC_VERSION_1_1:
        image_path_hint = "image_path";
        image_data_hint = "image_data";
    break;
    case EVENTD_LIBNOTIFY_SPEC_VERSION_OLD:
        image_path_hint = NULL; /* This is safe anyway since we check for that in get_image() */
        image_data_hint = "icon_data";
    break;
    }

    if ( image != NULL )
    {
        gint32 width, height, rowstride, bits, channels;
        gboolean alpha;
        GVariant *data;
        width = gdk_pixbuf_get_width(image);
        height = gdk_pixbuf_get_height(image);
        rowstride = gdk_pixbuf_get_rowstride(image);
        alpha = gdk_pixbuf_get_has_alpha(image);
        bits = gdk_pixbuf_get_bits_per_sample(image);
        channels = gdk_pixbuf_get_n_channels(image);
        data = g_variant_new_from_data(G_VARIANT_TYPE_BYTESTRING, gdk_pixbuf_read_pixels(image), gdk_pixbuf_get_byte_length(image), TRUE, NULL, NULL);
        g_variant_builder_add(hints, "{sv}", image_data_hint, g_variant_new("(iiibii@ay)", width, height, rowstride, alpha, bits, channels, data));
        g_object_unref(image);
    }
    if ( image_uri != NULL )
    {
        g_variant_builder_add(hints, "{sv}", image_path_hint, g_variant_new_string(image_uri));
        g_free(image_uri);
    }

    GVariantBuilder *actions;
    actions = g_variant_builder_new(G_VARIANT_TYPE_STRING_ARRAY);

    GVariant *args;
    args = g_variant_new("(susssasa{sv}i)",
        PACKAGE_NAME,
        (gint32) 0,
        ( icon_uri != NULL ) ? icon_uri : "",
        ( title != NULL ) ? title : "",
        ( message != NULL ) ? message : "",
        actions,
        hints,
        (gint32) eventd_event_get_timeout(event));
    g_free(icon_uri);
    g_free(message);
    g_free(title);

    EventdLibnotifyNotification *notification;
    notification = g_slice_new0(EventdLibnotifyNotification);
    notification->context = context;
    notification->event = g_object_ref(event);

    g_dbus_proxy_call(context->server, "Notify", args, G_DBUS_CALL_FLAGS_NONE, -1, NULL, _eventd_libnotify_proxy_notify, notification);
    g_signal_connect_swapped(notification->event, "ended", G_CALLBACK(_eventd_libnotify_event_ended), notification);
}


/*
 * Plugin interface
 */

EVENTD_EXPORT const gchar *eventd_plugin_id = "notify";
EVENTD_EXPORT
void
eventd_plugin_get_interface(EventdPluginInterface *interface)
{
    eventd_plugin_interface_add_init_callback(interface, _eventd_libnotify_init);
    eventd_plugin_interface_add_uninit_callback(interface, _eventd_libnotify_uninit);

    eventd_plugin_interface_add_start_callback(interface, _eventd_libnotify_start);
    eventd_plugin_interface_add_stop_callback(interface, _eventd_libnotify_stop);

    eventd_plugin_interface_add_action_parse_callback(interface, _eventd_libnotify_action_parse);
    eventd_plugin_interface_add_config_reset_callback(interface, _eventd_libnotify_config_reset);

    eventd_plugin_interface_add_event_action_callback(interface, _eventd_libnotify_event_action);
}
