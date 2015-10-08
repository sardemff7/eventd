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

#include <libnotify/notify.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libeventd-event.h>
#include <eventd-plugin.h>
#include <libeventd-helpers-config.h>

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

const gchar * const _eventd_libnotify_urgency[_EVENTD_LIBNOTIFY_URGENCY_SIZE] = {
    [EVENTD_LIBNOTIFY_URGENCY_LOW] =      "low",
    [EVENTD_LIBNOTIFY_URGENCY_NORMAL] =   "normal",
    [EVENTD_LIBNOTIFY_URGENCY_CRITICAL] = "critical",
};

static GdkPixbuf *
_eventd_libnotify_icon_get_pixbuf_from_file(const gchar *filename)
{
    GError *error = NULL;
    GdkPixbuf *pixbuf;

    pixbuf = gdk_pixbuf_new_from_file(filename, &error);
    if ( pixbuf == NULL )
    {
        g_warning("Couldn't load icon file: %s", error->message);
        g_clear_error(&error);
    }

    return pixbuf;
}

static GdkPixbuf *
_eventd_libnotify_icon_get_pixbuf_from_base64(const gchar *base64)
{
    GError *error = NULL;
    guchar *data;
    gsize length;
    GdkPixbufLoader *loader;
    GdkPixbuf *pixbuf = NULL;

    if ( base64 == NULL )
        return NULL;

    data = g_base64_decode(base64, &length);

    loader = gdk_pixbuf_loader_new();

    if ( ! gdk_pixbuf_loader_write(loader, data, length, &error) )
    {
        g_warning("Couldn't write icon data: %s", error->message);
        g_clear_error(&error);
        goto fail;
    }

    if ( ! gdk_pixbuf_loader_close(loader, &error) )
    {
        g_warning("Couldn't terminate icon data loading: %s", error->message);
        g_clear_error(&error);
        goto fail;
    }

    pixbuf = g_object_ref(gdk_pixbuf_loader_get_pixbuf(loader));

fail:
    g_free(data);
    g_object_unref(loader);
    return pixbuf;
}

static GdkPixbuf *
_eventd_libnotify_get_image(EventdPluginAction *action, EventdEvent *event, gboolean server_support, gchar **icon_uri)
{
    gchar *file;
    const gchar *data;
    GdkPixbuf *image = NULL;
    GdkPixbuf *icon = NULL;

    if ( evhelpers_filename_get_path(action->image, event, "icons", &data, &file) )
    {
        if ( file != NULL )
            image = _eventd_libnotify_icon_get_pixbuf_from_file(file);
        else if ( data != NULL )
            image = _eventd_libnotify_icon_get_pixbuf_from_base64(eventd_event_get_data(event, data));
        g_free(file);
    }

    if ( evhelpers_filename_get_path(action->icon, event, "icons", &data, &file) )
    {
        if ( file != NULL )
        {
            if ( ( image == NULL ) || ( server_support ) )
                *icon_uri = g_strconcat("file://", file, NULL);
            else
                icon = _eventd_libnotify_icon_get_pixbuf_from_file(file);
        }
        else if ( data != NULL )
            icon = _eventd_libnotify_icon_get_pixbuf_from_base64(eventd_event_get_data(event, data));
        g_free(file);
    }

    if ( ( image == NULL ) && ( icon == NULL ) )
        return NULL;

    if ( image == NULL )
        return icon;

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
_eventd_libnotify_event_ended(NotifyNotification *notification, EventdEventEndReason reason, EventdEvent *event)
{
    notify_notification_close(notification, NULL);
    g_object_unref(notification);
    g_object_unref(event);
}

static void
_eventd_libnotify_event_action(EventdPluginContext *context, EventdPluginAction *action, EventdEvent *event)
{
    if ( context->bus_name_owned )
        /* We would be sending stuff to ourselves */
        return;

    GError *error = NULL;
    gchar *title;
    gchar *message;
    gchar *icon_uri = NULL;
    NotifyNotification *notification = NULL;
    GdkPixbuf *image;

    title = evhelpers_format_string_get_string(action->title, event, NULL, NULL);
    message = evhelpers_format_string_get_string(action->message, event, NULL, NULL);

    image = _eventd_libnotify_get_image(action, event, context->client.overlay_icon, &icon_uri);

    notification = notify_notification_new(title, message, icon_uri);
    g_free(icon_uri);
    g_free(message);
    g_free(title);

    if ( image != NULL )
    {
        notify_notification_set_image_from_pixbuf(notification, image);
        g_object_unref(image);
    }

    notify_notification_set_urgency(notification, action->urgency);
    notify_notification_set_timeout(notification, eventd_event_get_timeout(event));

    if ( action->hints != NULL )
    {
        GHashTableIter iter;
        gchar *key;
        FormatString *value_;
        gchar *value;
        GVariant *variant;
        g_hash_table_iter_init(&iter, action->hints);
        while ( g_hash_table_iter_next(&iter, (gpointer *) &key, (gpointer *) &value_) )
        {
            value = evhelpers_format_string_get_string(value_, event, NULL, NULL);
            if ( *value == '\0' )
                notify_notification_set_hint(notification, key, NULL);
            else
            {
                variant = g_variant_parse(NULL, value, NULL, NULL, NULL);
                if ( variant != NULL )
                    notify_notification_set_hint(notification, key, variant);
            }
            g_free(value);
        }
    }

    if ( ! notify_notification_show(notification, &error) )
        g_warning("Can't show the notification: %s", error->message);
    g_clear_error(&error);

    g_signal_connect_swapped(g_object_ref(event), "ended", G_CALLBACK(_eventd_libnotify_event_ended), g_object_ref(notification));

    g_object_unref(notification);
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

