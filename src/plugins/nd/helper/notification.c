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

#include <glib.h>
#include <glib-object.h>

#include <libeventd-event.h>
#include <libeventd-config.h>
#include <libeventd-regex.h>

#include <eventd-nd-notification.h>

struct _EventdNdNotification {
    gchar *title;
    gchar *message;
    guchar *image;
    gsize image_length;
    const gchar *image_format;
    guchar *icon;
    gsize icon_length;
    const gchar *icon_format;
};

void
eventd_nd_notification_init()
{
    libeventd_regex_init();
}

void
eventd_nd_notification_uninit()
{
    libeventd_regex_clean();
}

static void
_eventd_nd_notification_icon_data_from_file(gchar *path, guchar **data, gsize *length)
{
    GError *error = NULL;

    if ( *path == 0 )
        return;

    if ( ! g_file_get_contents(path, (gchar **)data, length, &error) )
        g_warning("Couldn’t load file '%s': %s", path, error->message);
    g_clear_error(&error);

    g_free(path);
}

static void
_eventd_nd_notification_icon_data_from_base64(EventdEvent *event, const gchar *name, guchar **data, gsize *length, const gchar **format)
{
    const gchar *base64;
    gchar *format_name;

    base64 = eventd_event_get_data(event, name);
    if ( base64 != NULL )
        *data = g_base64_decode(base64, length);

    format_name = g_strconcat(name, "-format", NULL);
    *format = eventd_event_get_data(event, format_name);
    g_free(format_name);
}

EventdNdNotification *
eventd_nd_notification_new(EventdEvent *event, const gchar *title, const gchar *message, const gchar *image_name, const gchar *icon_name)
{
    EventdNdNotification *self;
    gchar *icon;

    self = g_new0(EventdNdNotification, 1);

    self->title = libeventd_regex_replace_event_data(title, event, NULL, NULL);

    self->message = libeventd_regex_replace_event_data(message, event, NULL, NULL);

    if ( ( icon = libeventd_config_get_filename(image_name, event, "icons") ) != NULL )
        _eventd_nd_notification_icon_data_from_file(icon, &self->image, &self->image_length);
    else
        _eventd_nd_notification_icon_data_from_base64(event, image_name, &self->image, &self->image_length, &self->image_format);

    if ( ( icon = libeventd_config_get_filename(icon_name, event, "icons") ) != NULL )
        _eventd_nd_notification_icon_data_from_file(icon, &self->icon, &self->icon_length);
    else
        _eventd_nd_notification_icon_data_from_base64(event, icon_name, &self->icon, &self->icon_length, &self->icon_format);

    return self;
}

void
eventd_nd_notification_free(EventdNdNotification *self)
{
    g_free(self->icon);
    g_free(self->image);
    g_free(self->message);
    g_free(self->title);

    g_free(self);
}

const gchar *
eventd_nd_notification_get_title(EventdNdNotification *self)
{
    return self->title;
}

const gchar *
eventd_nd_notification_get_message(EventdNdNotification *self)
{
    return self->message;
}

void
eventd_nd_notification_get_image(EventdNdNotification *self, const guchar **data, gsize *length, const gchar **format)
{
    *data = self->image;
    *length = self->image_length;
    *format = self->image_format;
}

void
eventd_nd_notification_get_icon(EventdNdNotification *self, const guchar **data, gsize *length, const gchar **format)
{
    *data = self->icon;
    *length = self->icon_length;
    *format = self->icon_format;
}

