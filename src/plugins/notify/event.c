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
#include <gio/gio.h>

#include <eventd-plugin.h>
#include <libeventd-event.h>
#include <libeventd-config.h>
#include <libeventd-regex.h>

#include "event.h"

struct _EventdPluginContext {
    GHashTable *events;
};

void
eventd_libnotify_event_update(EventdLibnotifyEvent *event, gboolean disable, const char *title, const char *message, const char *icon, const char *overlay_icon, Int *scale)
{
    event->disable = disable;
    if ( title != NULL )
    {
        g_free(event->title);
        event->title = g_strdup(title);
    }
    if ( message != NULL )
    {
        g_free(event->message);
        event->message = g_strdup(message);
    }
    if ( icon != NULL )
    {
        g_free(event->icon);
        event->icon = g_strdup(icon);
    }
    if ( overlay_icon != NULL )
    {
        g_free(event->overlay_icon);
        event->overlay_icon = g_strdup(overlay_icon);
    }
    if ( scale->set )
        event->scale = (gdouble)scale->value / 100.;
}

EventdLibnotifyEvent *
eventd_libnotify_event_new(gboolean disable, const char *title, const char *message, const char *icon, const char *overlay_icon, Int *scale, EventdLibnotifyEvent *parent)
{
    EventdLibnotifyEvent *event = NULL;

    title = ( title != NULL ) ? title : ( parent != NULL ) ? parent->title : "$client-name - $name";
    message = ( message != NULL ) ? message : ( parent != NULL ) ? parent->message : "$text";
    icon = ( icon != NULL ) ? icon : ( parent != NULL ) ? parent->icon : "icon";
    overlay_icon = ( overlay_icon != NULL ) ? overlay_icon : ( parent != NULL ) ? parent->overlay_icon : "overlay-icon";
    scale->value = scale->set ? scale->value : ( parent != NULL ) ? parent->scale * 100 : 50;
    scale->set = TRUE;

    event = g_new0(EventdLibnotifyEvent, 1);

    eventd_libnotify_event_update(event, disable, title, message, icon, overlay_icon, scale);

    return event;
}

void
eventd_libnotify_event_free(gpointer data)
{
    EventdLibnotifyEvent *event = data;

    g_free(event->icon);
    g_free(event->overlay_icon);
    g_free(event->message);
    g_free(event->title);
    g_free(event);
}
