/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011 Quentin "Sardem FF7" Glidic
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
#include <cairo.h>

#include <libeventd-event.h>

#include "../notification.h"

#include "types.h"
#include "bubble.h"
#include "style.h"
#include "backends/backend.h"

#include "daemon.h"

struct _EventdNdContext {
    EventdNdStyle *style;
    GHashTable *bubbles;
    GList *displays;
};

EventdNdContext *
eventd_nd_init()
{
    EventdNdContext *context = NULL;

    context = g_new0(EventdNdContext, 1);

    context->style = eventd_nd_style_new();

    context->bubbles = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, eventd_nd_bubble_free);

    return context;
}

void
eventd_nd_uninit(EventdNdContext *context)
{
    if ( context == NULL )
        return;

    g_hash_table_unref(context->bubbles);

    g_list_free_full(context->displays, eventd_nd_display_free);

    eventd_nd_style_free(context->style);

    g_free(context);
}

void
eventd_nd_control_command(EventdNdContext *context, const gchar *command)
{
    const gchar *target = command+20;
    EventdNdDisplay *display;

    if ( context == NULL )
        return;

    if ( ! g_str_has_prefix(command, "notification-daemon ") )
        return;

    display = eventd_nd_display_new(target);
    if ( display == NULL )
        g_warning("Couldn’t initialize display for '%s'", target);
    else
        context->displays = g_list_prepend(context->displays, display);
}

static void
_eventd_nd_event_ended(EventdEvent *event, EventdEventEndReason reason, EventdNdContext *context)
{
    EventdNdBubble *bubble;
    gpointer id;

    id = GUINT_TO_POINTER(eventd_event_get_id(event));
    bubble = g_hash_table_lookup(context->bubbles, id);

    eventd_nd_bubble_hide(bubble);

    g_hash_table_remove(context->bubbles, id);
}

void
eventd_nd_event_action(EventdNdContext *context, EventdEvent *event, EventdNotificationNotification *notification)
{
    EventdNdBubble *bubble;
    gpointer id;

    if ( context == NULL )
        return;

    id = GUINT_TO_POINTER(eventd_event_get_id(event));

    g_signal_connect(event, "ended", G_CALLBACK(_eventd_nd_event_ended), context);

    /*
     * TODO: Update an existing bubble
     */
    bubble = eventd_nd_bubble_new(notification, context->style, context->displays);

    eventd_nd_bubble_show(bubble);
    g_hash_table_insert(context->bubbles, id, bubble);
}
