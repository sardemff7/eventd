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
#include <cairo.h>

#include <libeventd-event.h>

#include "../notification.h"

#include "types.h"
#include "bubble.h"
#include "style.h"
#if ! DISABLE_GRAPHICAL_BACKENDS
#include "backends/graphical.h"
#endif /* ! DISABLE_GRAPHICAL_BACKENDS */
#if ! DISABLE_FRAMEBUFFER_BACKENDS
#include "backends/fb.h"
#endif /* ! DISABLE_FRAMEBUFFER_BACKENDS */

#include "daemon.h"

struct _EventdNdContext {
    EventdNdStyle *style;
    GHashTable *bubbles;
    GList *graphical_displays;
    GList *framebuffer_displays;
};

EventdNdContext *
eventd_nd_init()
{
    EventdNdContext *context = NULL;

    context = g_new0(EventdNdContext, 1);

    eventd_nd_bubble_init();

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

#if ! DISABLE_GRAPHICAL_BACKENDS
    g_list_free_full(context->graphical_displays, eventd_nd_graphical_display_free);
#endif /* ! DISABLE_GRAPHICAL_BACKENDS */
#if ! DISABLE_FRAMEBUFFER_BACKENDS
    g_list_free_full(context->framebuffer_displays, eventd_nd_fb_display_free);
#endif /* ! DISABLE_FRAMEBUFFER_BACKENDS */

    eventd_nd_style_free(context->style);

    eventd_nd_bubble_uninit();

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

#if ! DISABLE_FRAMEBUFFER_BACKENDS
    if ( g_str_has_prefix(target, FRAMEBUFFER_TARGET_PREFIX) )
    {
        display = eventd_nd_fb_display_new(target, context->style);
        if ( display == NULL )
            g_warning("Couldn’t initialize framebuffer display for '%s'", target);
        else
            context->framebuffer_displays = g_list_prepend(context->framebuffer_displays, display);
    }
    else
#endif /* ! DISABLE_FRAMEBUFFER_BACKENDS */
    {
#if ! DISABLE_GRAPHICAL_BACKENDS
        display = eventd_nd_graphical_display_new(target, context->style);
        if ( display == NULL )
            g_warning("Couldn’t initialize graphical display for '%s'", target);
        else
            context->graphical_displays = g_list_prepend(context->graphical_displays, display);
#endif /* ! DISABLE_GRAPHICAL_BACKENDS */
    }
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
    bubble = eventd_nd_bubble_new(notification, context->style, context->graphical_displays, context->framebuffer_displays);

    eventd_nd_bubble_show(bubble);
    g_hash_table_insert(context->bubbles, id, bubble);
}
