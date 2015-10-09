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

#include "fdo-notifications.h"

typedef enum {
    EVENTD_LIBNOTIFY_URGENCY_LOW,
    EVENTD_LIBNOTIFY_URGENCY_NORMAL,
    EVENTD_LIBNOTIFY_URGENCY_CRITICAL,
    _EVENTD_LIBNOTIFY_URGENCY_SIZE
} EventdLibnotifyUrgency;

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
_eventd_libnotify_get_image(EventdPluginAction *action, EventdEvent *event, gboolean server_support, gchar **icon_uri, gchar **image_uri)
{
    gchar *uri;
    GdkPixbuf *image = NULL;
    GdkPixbuf *icon = NULL;

    uri = evhelpers_filename_get_uri(action->image, event, "icons");
    if ( uri != NULL )
    {
        if ( g_str_has_prefix(uri, "file://") )
            *image_uri = uri;
        else
            image = eventd_nd_pixbuf_from_uri(uri, 0, 0);
    }

    uri = evhelpers_filename_get_uri(action->icon, event, "icons");
    if ( uri != NULL )
    {
        if ( g_str_has_prefix(uri, "file://") )
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
    if ( server_support && ( *icon_uri != NULL ) && ( ( *image_uri != NULL ) || ( image != NULL ) ) )
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

    ret = g_dbus_proxy_call_finish(self->context->client.server, res, &error);
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
        g_dbus_proxy_call(self->context->client.server, "CloseNotification", g_variant_new("(u)", self->id), G_DBUS_CALL_FLAGS_NONE, -1, NULL, _eventd_libnotify_notification_close, self);
    else
        _eventd_libnotify_notification_free(self);
}


/*
 * Start/stop interface
 */

static void
_eventd_libnotify_proxy_get_capabilities(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    EventdPluginContext *context = user_data;
    GError *error = NULL;
    GVariant *ret;

    ret = g_dbus_proxy_call_finish(context->client.server, res, &error);
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
            context->client.overlay_icon = TRUE;
    }

    g_free(capabilities);
    g_variant_unref(ret);
}

static void
_eventd_libnotify_proxy_create_callback(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    EventdPluginContext *context = user_data;
    GError *error = NULL;

    context->client.server = g_dbus_proxy_new_finish(res, &error);
    if ( context->client.server == NULL )
    {
        g_warning("Couldn't connection to org.freedesktop.Notifications server: %s", error->message);
        g_clear_error(&error);
        return;
    }

    g_dbus_proxy_call(context->client.server, "GetCapabilities", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, _eventd_libnotify_proxy_get_capabilities, context);
}

static void
_eventd_libnotify_bus_name_appeared(GDBusConnection *connection, const gchar *name, const gchar *name_owner, gpointer user_data)
{
    EventdPluginContext *context = user_data;
    GDBusProxyFlags flags = G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS|G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES;

    g_dbus_proxy_new(connection, flags, context->introspection_data->interfaces[0], name, NOTIFICATION_BUS_PATH, NOTIFICATION_BUS_NAME, NULL, _eventd_libnotify_proxy_create_callback, context);
}

static void
_eventd_libnotify_bus_name_vanished(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    EventdPluginContext *context = user_data;
    g_object_unref(context->client.server);
}

void
eventd_libnotify_start(EventdPluginContext *context)
{
    context->client.id = g_bus_watch_name(G_BUS_TYPE_SESSION, NOTIFICATION_BUS_NAME, G_BUS_NAME_WATCHER_FLAGS_NONE, _eventd_libnotify_bus_name_appeared, _eventd_libnotify_bus_name_vanished, context, NULL);
}

void
eventd_libnotify_stop(EventdPluginContext *context)
{
    g_bus_unwatch_name(context->client.id);
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

    context->client.actions = g_slist_prepend(context->client.actions, action);
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
    g_slist_free_full(context->client.actions, _eventd_libnotify_action_free);
    context->client.actions = NULL;
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

    ret = g_dbus_proxy_call_finish(self->context->client.server, res, &error);
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

    if ( context->client.server == NULL )
        return;

    gchar *title;
    gchar *message;
    gchar *icon_uri = NULL;
    gchar *image_uri = NULL;
    GdkPixbuf *image;

    title = evhelpers_format_string_get_string(action->title, event, NULL, NULL);
    message = evhelpers_format_string_get_string(action->message, event, NULL, NULL);

    image = _eventd_libnotify_get_image(action, event, context->client.overlay_icon, &icon_uri, &image_uri);

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
        g_variant_builder_add(hints, "{sv}", "image-data", g_variant_new("(iiibii@ay)", width, height, rowstride, alpha, bits, channels, data));
        g_object_unref(image);
    }
    if ( image_uri != NULL )
    {
        g_variant_builder_add(hints, "{s<s>}", "image-path", image_uri);
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

    g_dbus_proxy_call(context->client.server, "Notify", args, G_DBUS_CALL_FLAGS_NONE, -1, NULL, _eventd_libnotify_proxy_notify, notification);
    g_signal_connect_swapped(notification->event, "ended", G_CALLBACK(_eventd_libnotify_event_ended), notification);
}


/*
 * Plugin interface
 */

void
eventd_libnotify_get_interface(EventdPluginInterface *interface)
{
    eventd_plugin_interface_add_action_parse_callback(interface, _eventd_libnotify_action_parse);
    eventd_plugin_interface_add_config_reset_callback(interface, _eventd_libnotify_config_reset);

    eventd_plugin_interface_add_event_action_callback(interface, _eventd_libnotify_event_action);
}

