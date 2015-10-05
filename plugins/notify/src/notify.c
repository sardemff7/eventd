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

struct _EventdPluginContext {
    GSList *actions;
};

struct _EventdPluginAction {
    FormatString *title;
    FormatString *message;
    Filename *image;
    Filename *icon;
    gdouble scale;
    NotifyUrgency urgency;
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
_eventd_libnotify_get_image(EventdPluginAction *action, EventdEvent *event, gchar **icon_uri)
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
            *icon_uri = g_strconcat("file://", file, NULL);
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
_eventd_libnotify_event_new(FormatString *title, FormatString *message, Filename *image, Filename *icon, gint64 scale, gchar *urgency)
{
    EventdPluginAction *event;

    event = g_slice_new0(EventdPluginAction);

    event->title = title;
    event->message = message;
    event->image = image;
    event->icon = icon;
    event->scale = (gdouble) scale / 100.;
    event->urgency = NOTIFY_URGENCY_NORMAL;
    if ( g_strcmp0(urgency, "low") == 0 )
        event->urgency = NOTIFY_URGENCY_LOW;
    else if ( g_strcmp0(urgency, "normal") == 0 )
        event->urgency = NOTIFY_URGENCY_NORMAL;
    else if ( g_strcmp0(urgency, "critical") == 0 )
        event->urgency = NOTIFY_URGENCY_CRITICAL;
    else
        g_warning("Unknown urgency: %s", urgency);
    g_free(urgency);

    return event;
}

static void
_eventd_libnotify_action_free(gpointer data)
{
    EventdPluginAction *event = data;

    evhelpers_filename_unref(event->image);
    evhelpers_filename_unref(event->icon);
    evhelpers_format_string_unref(event->message);
    evhelpers_format_string_unref(event->title);

    g_slice_free(EventdPluginAction, event);
}


/*
 * Initialization interface
 */

static EventdPluginContext *
_eventd_libnotify_init(EventdPluginCoreContext *core, EventdPluginCoreInterface *interface)
{
    EventdPluginContext *context;

    notify_init(PACKAGE_NAME);

    if ( ! notify_is_initted() )
    {
        g_warning("Couldn't initialize notify system");
        return NULL;
    }

    gchar *server_name;
    notify_get_server_info(&server_name, NULL, NULL, NULL);

    if ( g_strcmp0(server_name, PACKAGE_NAME) == 0 )
    {
        g_debug("We would send notifications to ourselves: quitting");

        g_free(server_name);
        notify_uninit();
        return NULL;
    }
    g_free(server_name);

    context = g_new0(EventdPluginContext, 1);

    return context;
}

static void
_eventd_libnotify_uninit(EventdPluginContext *context)
{
    g_free(context);

    notify_uninit();
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
    gchar *urgency = NULL;

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
    if ( evhelpers_config_key_file_get_string_with_default(config_file, "Libnotify", "Urgency", "normal", &urgency) < 0 )
        goto skip;

    EventdPluginAction *action;
    action = _eventd_libnotify_event_new(title, message, image, icon, scale, urgency);
    title = message = NULL;
    image = icon = NULL;
    urgency = NULL;

    context->actions = g_slist_prepend(context->actions, action);
    return action;

skip:
    g_free(urgency);
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
_eventd_libnotify_event_action(EventdPluginContext *context, EventdPluginAction *action, EventdEvent *event)
{
    GError *error = NULL;
    gchar *title;
    gchar *message;
    gchar *icon_uri = NULL;
    NotifyNotification *notification = NULL;
    GdkPixbuf *image;

    title = evhelpers_format_string_get_string(action->title, event, NULL, NULL);
    message = evhelpers_format_string_get_string(action->message, event, NULL, NULL);

    image = _eventd_libnotify_get_image(action, event, &icon_uri);

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

    if ( ! notify_notification_show(notification, &error) )
        g_warning("Can't show the notification: %s", error->message);
    g_clear_error(&error);

    g_object_unref(notification);
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

    eventd_plugin_interface_add_action_parse_callback(interface, _eventd_libnotify_action_parse);
    eventd_plugin_interface_add_config_reset_callback(interface, _eventd_libnotify_config_reset);

    eventd_plugin_interface_add_event_action_callback(interface, _eventd_libnotify_event_action);
}

