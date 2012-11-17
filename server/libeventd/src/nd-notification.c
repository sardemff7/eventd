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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <glib.h>
#include <glib-object.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libeventd-event.h>
#include <libeventd-config.h>
#include <libeventd-regex.h>

#include <libeventd-nd-notification.h>

struct _LibeventdNdNotificationTemplate {
    gchar *title;
    gchar *message;
    gchar *image;
    gchar *icon;
};

struct _LibeventdNdNotification {
    gchar *title;
    gchar *message;
    GdkPixbuf *image;
    GdkPixbuf *icon;
};

EVENTD_EXPORT
void
libeventd_nd_notification_init()
{
    libeventd_regex_init();
}

EVENTD_EXPORT
void
libeventd_nd_notification_uninit()
{
    libeventd_regex_clean();
}


EVENTD_EXPORT
LibeventdNdNotificationTemplate *
libeventd_nd_notification_template_new(GKeyFile *config_file)
{
    LibeventdNdNotificationTemplate *self;

    self = g_new(LibeventdNdNotificationTemplate, 1);

    if ( ! g_key_file_has_group(config_file, "Notification") )
    {
        self->title = g_strdup("$name");
        self->message = g_strdup("$text");
        self->image = g_strdup("image");
        self->icon = g_strdup("icon");
        return self;
    }

    gchar *string;

    if ( libeventd_config_key_file_get_string(config_file, "Notification", "Title", &string) == 0 )
        self->title = string;
    else
        self->title = g_strdup("$name");

    if ( libeventd_config_key_file_get_string(config_file, "Notification", "Message", &string) == 0 )
        self->message = string;
    else
        self->message = g_strdup("$text");

    if ( libeventd_config_key_file_get_string(config_file, "Notification", "Image", &string) == 0 )
        self->image = string;
    else
        self->image = g_strdup("image");

    if ( libeventd_config_key_file_get_string(config_file, "Notification", "Icon", &string) == 0 )
        self->icon = string;
    else
        self->icon = g_strdup("icon");

    return self;
}

EVENTD_EXPORT
void
libeventd_nd_notification_template_free(LibeventdNdNotificationTemplate *self)
{
    g_free(self->image);
    g_free(self->icon);
    g_free(self->message);
    g_free(self->title);
    g_free(self);
}


static GdkPixbuf *
_libeventd_nd_notification_pixbuf_from_file(const gchar *path, gint width, gint height)
{
    GError *error = NULL;
    GdkPixbufFormat *format;
    GdkPixbuf *pixbuf;

    if ( *path == 0 )
        return NULL;

    if ( ( ( width > 0 ) || ( height > 0 ) ) && ( ( format = gdk_pixbuf_get_file_info(path, NULL, NULL) ) != NULL ) && gdk_pixbuf_format_is_scalable(format) )
        pixbuf = gdk_pixbuf_new_from_file_at_size(path, width, height, &error);
    else
        pixbuf = gdk_pixbuf_new_from_file(path, &error);

    if ( pixbuf == NULL )
        g_warning("Couldn't load file '%s': %s", path, error->message);
    g_clear_error(&error);

    return pixbuf;
}

static void
_libeventd_nd_notification_pixbuf_data_free(guchar *pixels, gpointer data)
{
    g_free(pixels);
}

static GdkPixbuf *
_libeventd_nd_notification_pixbuf_from_base64(EventdEvent *event, const gchar *name)
{
    GdkPixbuf *pixbuf = NULL;
    const gchar *base64;
    guchar *data;
    gsize length;
    const gchar *format;
    gchar *format_name;

    base64 = eventd_event_get_data(event, name);
    if ( base64 == NULL )
        return NULL;
    data = g_base64_decode(base64, &length);

    format_name = g_strconcat(name, "-format", NULL);
    format = eventd_event_get_data(event, format_name);
    g_free(format_name);

    if ( format != NULL )
    {
        gint width, height;
        gint stride;
        gboolean alpha;
        gchar *f;

        width = g_ascii_strtoll(format, &f, 16);
        height = g_ascii_strtoll(f+1, &f, 16);
        stride = g_ascii_strtoll(f+1, &f, 16);
        alpha = g_ascii_strtoll(f+1, &f, 16);

        pixbuf = gdk_pixbuf_new_from_data(data, GDK_COLORSPACE_RGB, alpha, 8, width, height, stride, _libeventd_nd_notification_pixbuf_data_free, NULL);
    }
    else
    {
        GError *error = NULL;
        GdkPixbufLoader *loader;

        loader = gdk_pixbuf_loader_new();

        if ( ! gdk_pixbuf_loader_write(loader, data, length, &error) )
        {
            g_warning("Couldn't write image data: %s", error->message);
            g_clear_error(&error);
            goto error;
        }

        if ( ! gdk_pixbuf_loader_close(loader, &error) )
        {
            g_warning("Couldn't load image data: %s", error->message);
            g_clear_error(&error);
            goto error;
        }

        pixbuf = g_object_ref(gdk_pixbuf_loader_get_pixbuf(loader));

    error:
        g_object_unref(loader);
        g_free(data);
    }

    return pixbuf;
}

EVENTD_EXPORT
LibeventdNdNotification *
libeventd_nd_notification_new(LibeventdNdNotificationTemplate *template, EventdEvent *event, gint width, gint height)
{
    LibeventdNdNotification *self;
    gchar *path;

    self = g_new0(LibeventdNdNotification, 1);

    self->title = libeventd_regex_replace_event_data(template->title, event, NULL, NULL);

    self->message = libeventd_regex_replace_event_data(template->message, event, NULL, NULL);

    if ( ( path = libeventd_config_get_filename(template->image, event, "icons") ) != NULL )
    {
        self->image = _libeventd_nd_notification_pixbuf_from_file(path, width, height);
        g_free(path);
    }
    else
       self->image =  _libeventd_nd_notification_pixbuf_from_base64(event, template->image);

    if ( ( path = libeventd_config_get_filename(template->icon, event, "icons") ) != NULL )
    {
        self->icon = _libeventd_nd_notification_pixbuf_from_file(path, width, height);
        g_free(path);
    }
    else
        self->icon = _libeventd_nd_notification_pixbuf_from_base64(event, template->icon);

    return self;
}

EVENTD_EXPORT
void
libeventd_nd_notification_free(LibeventdNdNotification *self)
{
    if ( self == NULL )
        return;

    if ( self->icon != NULL )
        g_object_unref(self->icon);
    if ( self->image != NULL )
        g_object_unref(self->image);
    g_free(self->message);
    g_free(self->title);

    g_free(self);
}

EVENTD_EXPORT
const gchar *
libeventd_nd_notification_get_title(LibeventdNdNotification *self)
{
    return self->title;
}

EVENTD_EXPORT
const gchar *
libeventd_nd_notification_get_message(LibeventdNdNotification *self)
{
    return self->message;
}

EVENTD_EXPORT
GdkPixbuf *
libeventd_nd_notification_get_image(LibeventdNdNotification *self)
{
    return self->image;
}

EVENTD_EXPORT
GdkPixbuf *
libeventd_nd_notification_get_icon(LibeventdNdNotification *self)
{
    return self->icon;
}

