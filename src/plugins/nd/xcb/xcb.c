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

#include <stdlib.h>
#include <glib.h>
#include <glib-object.h>

#include <cairo.h>

#include <cairo-xcb.h>
#include <xcb/xcb.h>
#include <libxcb-glib.h>
#include <xcb/shape.h>

#include <libeventd-event.h>

#include <eventd-nd-backend.h>

struct _EventdNdBackendContext {
    EventdNdContext *nd;
    EventdNdInterface *nd_interface;
    GSList *displays;
};

struct _EventdNdDisplay {
    EventdNdBackendContext *context;
    GXcbSource *source;
    xcb_connection_t *xcb_connection;
    xcb_screen_t *screen;
    gboolean shape;
    GHashTable *bubbles;
};

struct _EventdNdSurface {
    EventdEvent *event;
    EventdNdDisplay *display;
    xcb_window_t window;
    gint width;
    gint height;
    cairo_surface_t *bubble;
};

static EventdNdBackendContext *
_eventd_nd_xcb_init(EventdNdContext *nd, EventdNdInterface *nd_interface)
{
    EventdNdBackendContext *context;

    context = g_new0(EventdNdBackendContext, 1);

    context->nd = nd;
    context->nd_interface = nd_interface;

    return context;
}

static void
_eventd_nd_xcb_uninit(EventdNdBackendContext *context)
{
    g_slist_free_full(context->displays, g_free);

    g_free(context);
}

static xcb_visualtype_t *
get_root_visual_type(xcb_screen_t *s)
{
    xcb_visualtype_t *visual_type = NULL;
    xcb_depth_iterator_t depth_iter;

    for ( depth_iter = xcb_screen_allowed_depths_iterator(s) ; depth_iter.rem ; xcb_depth_next(&depth_iter) )
    {
        xcb_visualtype_iterator_t visual_iter;
        for ( visual_iter = xcb_depth_visuals_iterator(depth_iter.data) ; visual_iter.rem ; xcb_visualtype_next(&visual_iter) )
        {
            if ( s->root_visual == visual_iter.data->visual_id )
            {
                visual_type = visual_iter.data;
                break;
            }
        }
    }

    return visual_type;
}

static void _eventd_nd_xcb_surface_expose_event(EventdNdSurface *self, xcb_expose_event_t *event);
static void _eventd_nd_xcb_surface_button_release_event(EventdNdSurface *self);


static void
_eventd_nd_xcb_events_callback(xcb_generic_event_t *event, gpointer user_data)
{
    EventdNdDisplay *display = user_data;
    EventdNdSurface *surface;

    switch ( event->response_type & ~0x80 )
    {
    case XCB_EXPOSE:
    {
        xcb_expose_event_t *e = (xcb_expose_event_t *)event;

        surface = g_hash_table_lookup(display->bubbles, GUINT_TO_POINTER(e->window));
        if ( surface != NULL )
            _eventd_nd_xcb_surface_expose_event(surface, e);
    }
    break;
    case XCB_BUTTON_RELEASE:
    {
        xcb_button_release_event_t *e = (xcb_button_release_event_t *)event;

        surface = g_hash_table_lookup(display->bubbles, GUINT_TO_POINTER(e->event));
        if ( surface != NULL )
            _eventd_nd_xcb_surface_button_release_event(surface);
    }
    break;
    default:
    break;
    }
}

static gboolean
_eventd_nd_xcb_display_test(EventdNdBackendContext *context, const gchar *target)
{
    gint r;
    gchar *host;
    gint display;

    r = xcb_parse_display(target, &host, &display, NULL);
    if ( r == 0 )
        return FALSE;
    free(host);

    if ( g_slist_find_custom(context->displays, target, g_str_equal) != NULL )
        return FALSE;

    return TRUE;
}

static void
_eventd_nd_xcb_display_error_callback(gpointer user_data)
{
    EventdNdDisplay *display = user_data;

    display->context->nd_interface->remove_display(display->context->nd, display);
}

static EventdNdDisplay *
_eventd_nd_xcb_display_new(EventdNdBackendContext *context, const gchar *target)
{
    EventdNdDisplay *display;
    const xcb_query_extension_reply_t *shape_query;

    display = g_new0(EventdNdDisplay, 1);

    display->source = g_xcb_source_new(NULL, target, NULL, _eventd_nd_xcb_events_callback, display, NULL);
    if ( display->source == NULL )
    {
        g_free(display);
        return NULL;
    }
    display->context = context;
    g_xcb_source_set_error_callback(display->source, _eventd_nd_xcb_display_error_callback);

    context->displays = g_slist_prepend(context->displays, g_strdup(target));

    display->xcb_connection = g_xcb_source_get_connection(display->source);

    display->screen = xcb_setup_roots_iterator(xcb_get_setup(display->xcb_connection)).data;

    shape_query = xcb_get_extension_data(display->xcb_connection, &xcb_shape_id);
    if ( ! shape_query->present )
        g_warning("No Shape extension");
    else
        display->shape = TRUE;

    display->bubbles = g_hash_table_new(NULL, NULL);

    return display;
}

static void
_eventd_nd_xcb_display_free(EventdNdDisplay *context)
{
    g_hash_table_unref(context->bubbles);
    g_xcb_source_unref(context->source);
    g_free(context);
}

static void
_eventd_nd_xcb_surface_expose_event(EventdNdSurface *self, xcb_expose_event_t *event)
{
    cairo_surface_t *cs;
    cairo_t *cr;

    cs = cairo_xcb_surface_create(self->display->xcb_connection, self->window, get_root_visual_type(self->display->screen), self->width, self->height);
    cr = cairo_create(cs);
    cairo_set_source_surface(cr, self->bubble, 0, 0);
    cairo_rectangle(cr,  event->x, event->y, event->width, event->height);
    cairo_fill(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(cs);
}

static void
_eventd_nd_xcb_surface_button_release_event(EventdNdSurface *self)
{
    eventd_event_end(self->event, EVENTD_EVENT_END_REASON_USER_DISMISS);
}

static EventdNdSurface *
_eventd_nd_xcb_surface_new(EventdEvent *event, EventdNdDisplay *display, cairo_surface_t *bubble, cairo_surface_t *shape)
{
    guint32 selmask = XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
    guint32 selval[] = { 1, XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_RELEASE };
    EventdNdSurface *surface;

    gint width;
    gint height;

    width = cairo_image_surface_get_width(bubble);
    height = cairo_image_surface_get_height(bubble);

    surface = g_new0(EventdNdSurface, 1);

    surface->event = g_object_ref(event);

    surface->display = display;
    surface->width = width;
    surface->height = height;
    surface->bubble = cairo_surface_reference(bubble);

    surface->window = xcb_generate_id(display->xcb_connection);
    xcb_create_window(display->xcb_connection,
                                       display->screen->root_depth,   /* depth         */
                                       surface->window,
                                       display->screen->root,         /* parent window */
                                       0, 0,                          /* x, y          */
                                       width, height,                 /* width, height */
                                       0,                             /* border_width  */
                                       XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class         */
                                       display->screen->root_visual,  /* visual        */
                                       selmask, selval);              /* masks         */

    if ( display->shape )
    {
        xcb_pixmap_t shape_id;
        xcb_gcontext_t gc;

        shape_id = xcb_generate_id(display->xcb_connection);
        xcb_create_pixmap(display->xcb_connection, 1,
                          shape_id, display->screen->root,
                          width, height);

        gc = xcb_generate_id(display->xcb_connection);
        xcb_create_gc(display->xcb_connection, gc, shape_id, 0, NULL);
        xcb_put_image(display->xcb_connection, XCB_IMAGE_FORMAT_Z_PIXMAP, shape_id, gc, width, height, 0, 0, 0, 1, cairo_image_surface_get_stride(shape) * height, cairo_image_surface_get_data(shape));
        xcb_free_gc(display->xcb_connection, gc);

        xcb_shape_mask(display->xcb_connection,
                       XCB_SHAPE_SO_INTERSECT, XCB_SHAPE_SK_BOUNDING,
                       surface->window, 0, 0, shape_id);

        xcb_free_pixmap(display->xcb_connection, shape_id);
    }

    xcb_map_window(surface->display->xcb_connection, surface->window);

    g_hash_table_insert(surface->display->bubbles, GUINT_TO_POINTER(surface->window), surface);

    return surface;
}

static void
_eventd_nd_xcb_surface_free(EventdNdSurface *surface)
{
    if ( surface == NULL )
        return;

    EventdNdDisplay *display = surface->display;

    g_hash_table_remove(display->bubbles, GUINT_TO_POINTER(surface->window));

    if ( ! g_source_is_destroyed((GSource *)display->source) )
    {
        xcb_unmap_window(display->xcb_connection, surface->window);
        xcb_flush(display->xcb_connection);
    }

    cairo_surface_destroy(surface->bubble);

    g_object_unref(surface->event);

    g_free(surface);
}

EVENTD_EXPORT const gchar *eventd_nd_backend_id = "eventd-nd-xcb";
EVENTD_EXPORT
void
eventd_nd_backend_get_info(EventdNdBackend *backend)
{
    backend->init = _eventd_nd_xcb_init;
    backend->uninit = _eventd_nd_xcb_uninit;

    backend->display_test = _eventd_nd_xcb_display_test;
    backend->display_new = _eventd_nd_xcb_display_new;
    backend->display_free = _eventd_nd_xcb_display_free;

    backend->surface_new  = _eventd_nd_xcb_surface_new;
    backend->surface_free = _eventd_nd_xcb_surface_free;
}
