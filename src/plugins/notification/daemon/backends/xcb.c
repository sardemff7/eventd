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

#include <cairo.h>

#include <cairo-xcb.h>
#include <xcb/xcb.h>
#include <libxcb-glib.h>
#include <xcb/shape.h>

#include "../types.h"
#include "../style.h"

#include "backend.h"

struct _EventdNdBackendContext {
    GSList *displays;
};

struct _EventdNdDisplay {
    GXcbSource *source;
    xcb_connection_t *xcb_connection;
    xcb_screen_t *screen;
    gint x;
    gint y;
    gboolean shape;
};

struct _EventdNdSurface {
    EventdNdDisplay *display;
    xcb_window_t window;
};

static EventdNdBackendContext *
_eventd_nd_xcb_init()
{
    EventdNdBackendContext *context;

    context = g_new0(EventdNdBackendContext, 1);

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

static EventdNdDisplay *
_eventd_nd_xcb_display_new(EventdNdBackendContext *context, const gchar *target, EventdNdStyleAnchor anchor, gint margin)
{
    EventdNdDisplay *display;
    GXcbSource *source;
    const xcb_query_extension_reply_t *shape_query;

    source = g_xcb_source_new(NULL, target, NULL);
    if ( source == NULL )
        return NULL;

    context->displays = g_slist_prepend(context->displays, g_strdup(target));

    display = g_new0(EventdNdDisplay, 1);

    display->source = source;
    display->xcb_connection = g_xcb_source_get_connection(source);

    display->screen = xcb_setup_roots_iterator(xcb_get_setup(display->xcb_connection)).data;

    shape_query = xcb_get_extension_data(display->xcb_connection, &xcb_shape_id);
    if ( ! shape_query->present )
        g_warning("No Shape extension");
    else
        display->shape = TRUE;

    switch ( anchor )
    {
    case EVENTD_ND_STYLE_ANCHOR_TOP_LEFT:
        display->x = margin;
        display->y = margin;
    break;
    case EVENTD_ND_STYLE_ANCHOR_TOP_RIGHT:
        display->x = - display->screen->width_in_pixels + margin;
        display->y = margin;
    break;
    case EVENTD_ND_STYLE_ANCHOR_BOTTOM_LEFT:
        display->x = margin;
        display->y = - display->screen->height_in_pixels + margin;
    break;
    case EVENTD_ND_STYLE_ANCHOR_BOTTOM_RIGHT:
        display->x = - display->screen->width_in_pixels + margin;
        display->y = - display->screen->height_in_pixels + margin;
    break;
    }

    return display;
}

static void
_eventd_nd_xcb_display_free(EventdNdDisplay *context)
{
    g_xcb_source_unref(context->source);
    g_free(context);
}

static EventdNdSurface *
_eventd_nd_xcb_surface_new(EventdNdDisplay *display, gint width, gint height, cairo_surface_t *bubble, cairo_surface_t *shape)
{
    guint32 selmask = XCB_CW_OVERRIDE_REDIRECT;
    guint32 selval[] = { 1 };
    gint x = 0, y = 0;
    EventdNdSurface *surface;
    cairo_surface_t *cs;
    cairo_t *cr;

    surface = g_new0(EventdNdSurface, 1);

    surface->display = display;

    x = display->x;
    y = display->y;

    if ( x < 0 )
        x = - x - width;
    if ( y < 0 )
        y = - y - height;

    surface->window = xcb_generate_id(display->xcb_connection);
    xcb_create_window(display->xcb_connection,
                      display->screen->root_depth,   /* depth         */
                      surface->window,
                      display->screen->root,         /* parent window */
                      x, y,                          /* x, y          */
                      width, height,                 /* width, height */
                      0,                             /* border_width  */
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class         */
                      display->screen->root_visual,  /* visual        */
                      selmask, selval);              /* masks         */
    xcb_map_window(display->xcb_connection, surface->window);

    cs = cairo_xcb_surface_create(display->xcb_connection, surface->window, get_root_visual_type(display->screen), width, height);
    cr = cairo_create(cs);
    cairo_set_source_surface(cr, bubble, 0, 0);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(cs);

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

    return surface;
}

static void
_eventd_nd_xcb_surface_show(EventdNdSurface *surface)
{
    xcb_map_window(surface->display->xcb_connection, surface->window);
    xcb_flush(surface->display->xcb_connection);
}

static void
_eventd_nd_xcb_surface_hide(EventdNdSurface *surface)
{
    xcb_unmap_window(surface->display->xcb_connection, surface->window);
    xcb_flush(surface->display->xcb_connection);
}

static void
_eventd_nd_xcb_surface_free(EventdNdSurface *surface)
{
    if ( surface == NULL )
        return;

    xcb_destroy_window(surface->display->xcb_connection, surface->window);
    xcb_flush(surface->display->xcb_connection);

    g_free(surface);
}

void
eventd_nd_backend_init(EventdNdBackend *backend)
{
    backend->init = _eventd_nd_xcb_init;
    backend->uninit = _eventd_nd_xcb_uninit;

    backend->display_test = _eventd_nd_xcb_display_test;
    backend->display_new = _eventd_nd_xcb_display_new;
    backend->display_free = _eventd_nd_xcb_display_free;

    backend->surface_new = _eventd_nd_xcb_surface_new;
    backend->surface_free = _eventd_nd_xcb_surface_free;
    backend->surface_show = _eventd_nd_xcb_surface_show;
    backend->surface_hide = _eventd_nd_xcb_surface_hide;
}
