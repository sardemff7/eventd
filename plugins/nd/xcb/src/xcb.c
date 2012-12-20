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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include <glib.h>
#include <glib-object.h>

#include <cairo.h>

#include <cairo-xcb.h>
#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <libxcb-glib.h>
#include <xcb/randr.h>
#include <xcb/shape.h>

#include <libeventd-event.h>
#include <libeventd-config.h>

#include <eventd-nd-backend.h>

struct _EventdNdBackendContext {
    EventdNdContext *nd;
    EventdNdInterface *nd_interface;
    gchar **outputs;
    GSList *displays;
};

struct _EventdNdDisplay {
    EventdNdBackendContext *context;
    GXcbSource *source;
    xcb_connection_t *xcb_connection;
    xcb_screen_t *screen;
    struct {
        gint x;
        gint y;
        gint width;
        gint height;
    } base_geometry;
    gint randr_event_base;
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


static void
_eventd_nd_xcb_global_parse(EventdNdBackendContext *context, GKeyFile *config_file)
{
    if ( ! g_key_file_has_group(config_file, "NotificationXcb") )
        return;

    libeventd_config_key_file_get_string_list(config_file, "NotificationXcb", "Outputs", &context->outputs, NULL);
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

static void
_eventd_nd_xcb_geometry_fallback(EventdNdDisplay *display)
{
    display->base_geometry.width = display->screen->width_in_pixels;
    display->base_geometry.height = display->screen->height_in_pixels;
}

static gboolean
_eventd_nd_xcb_randr_check_primary(EventdNdDisplay *display)
{
    xcb_randr_get_output_primary_cookie_t pcookie;
    xcb_randr_get_output_primary_reply_t *primary;

    pcookie = xcb_randr_get_output_primary(display->xcb_connection, display->screen->root);
    if ( ( primary = xcb_randr_get_output_primary_reply(display->xcb_connection, pcookie, NULL) ) == NULL )
        return FALSE;

    gboolean found = FALSE;

    xcb_randr_get_output_info_cookie_t ocookie;
    xcb_randr_get_output_info_reply_t *output;

    ocookie = xcb_randr_get_output_info(display->xcb_connection, primary->output, 0);
    if ( ( output = xcb_randr_get_output_info_reply(display->xcb_connection, ocookie, NULL) ) != NULL )
    {

        xcb_randr_get_crtc_info_cookie_t ccookie;
        xcb_randr_get_crtc_info_reply_t *crtc;

        ccookie = xcb_randr_get_crtc_info(display->xcb_connection, output->crtc, output->timestamp);
        if ( ( crtc = xcb_randr_get_crtc_info_reply(display->xcb_connection, ccookie, NULL) ) != NULL )
        {
            found = TRUE;

            display->base_geometry.x = crtc->x;
            display->base_geometry.y = crtc->y;
            display->base_geometry.width = crtc->width;
            display->base_geometry.height = crtc->height;

            free(crtc);
        }
        free(output);
    }
    free(primary);

    return found;
}

typedef struct {
    xcb_randr_get_output_info_reply_t *output;
    xcb_randr_get_crtc_info_reply_t *crtc;
} EventdNdXcbRandrOutput;

static gboolean
_eventd_nd_xcb_randr_check_outputs(EventdNdDisplay *display)
{
    xcb_randr_get_screen_resources_current_cookie_t rcookie;
    xcb_randr_get_screen_resources_current_reply_t *ressources;

    rcookie = xcb_randr_get_screen_resources_current(display->xcb_connection, display->screen->root);
    if ( ( ressources = xcb_randr_get_screen_resources_current_reply(display->xcb_connection, rcookie, NULL) ) == NULL )
    {
        g_warning("Couldn't get RandR screen ressources");
        return FALSE;
    }

    xcb_timestamp_t cts;
    xcb_randr_output_t *randr_outputs;
    gint i, length;

    cts = ressources->config_timestamp;

    length = xcb_randr_get_screen_resources_current_outputs_length(ressources);
    randr_outputs = xcb_randr_get_screen_resources_current_outputs(ressources);

    EventdNdXcbRandrOutput *outputs;
    EventdNdXcbRandrOutput *output;

    outputs = g_new(EventdNdXcbRandrOutput, length + 1);
    output = outputs;

    for ( i = 0 ; i < length ; ++i )
    {
        xcb_randr_get_output_info_cookie_t ocookie;

        ocookie = xcb_randr_get_output_info(display->xcb_connection, randr_outputs[i], cts);
        if ( ( output->output = xcb_randr_get_output_info_reply(display->xcb_connection, ocookie, NULL) ) == NULL )
            continue;

        xcb_randr_get_crtc_info_cookie_t ccookie;

        ccookie = xcb_randr_get_crtc_info(display->xcb_connection, output->output->crtc, cts);
        if ( ( output->crtc = xcb_randr_get_crtc_info_reply(display->xcb_connection, ccookie, NULL) ) == NULL )
            free(output->output);
        else
            ++output;
    }
    output->output = NULL;

    gchar **config_output;
    gboolean found = FALSE;
    for ( config_output = display->context->outputs ; ( *config_output != NULL ) && ( ! found ) ; ++config_output )
    {
        for ( output = outputs ; ( output->output != NULL ) && ( ! found ) ; ++output )
        {
            if ( g_ascii_strncasecmp(*config_output, (const gchar *)xcb_randr_get_output_info_name(output->output), xcb_randr_get_output_info_name_length(output->output)) != 0 )
                continue;
            display->base_geometry.x = output->crtc->x;
            display->base_geometry.y = output->crtc->y;
            display->base_geometry.width = output->crtc->width;
            display->base_geometry.height = output->crtc->height;

            found = TRUE;
        }
    }

    for ( output = outputs ; output->output != NULL ; ++output )
    {
        free(output->crtc);
        free(output->output);
    }
    g_free(outputs);

    return found;
}

static void
_eventd_nd_xcb_randr_check_geometry(EventdNdDisplay *display)
{
    gboolean found = FALSE;
    if ( display->context->outputs != NULL )
        found = _eventd_nd_xcb_randr_check_outputs(display);
    if ( ! found )
        found = _eventd_nd_xcb_randr_check_primary(display);
    if ( ! found )
        _eventd_nd_xcb_geometry_fallback(display);
}

static void _eventd_nd_xcb_surface_expose_event(EventdNdSurface *self, xcb_expose_event_t *event);
static void _eventd_nd_xcb_surface_button_release_event(EventdNdSurface *self);


static gboolean
_eventd_nd_xcb_events_callback(xcb_generic_event_t *event, gpointer user_data)
{
    EventdNdDisplay *display = user_data;
    EventdNdSurface *surface;

    if ( event == NULL )
    {
        display->context->nd_interface->remove_display(display->context->nd, display);
        return FALSE;
    }

    gint type = event->response_type & ~0x80;

    switch ( type - display->randr_event_base )
    {
    case XCB_RANDR_SCREEN_CHANGE_NOTIFY:
        _eventd_nd_xcb_randr_check_outputs(display);
    break;
    case XCB_RANDR_NOTIFY:
    break;
    default:
    switch ( type )
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

    return TRUE;
}

static EventdNdDisplay *
_eventd_nd_xcb_display_new(EventdNdBackendContext *context, const gchar *target)
{
    if ( target == NULL )
        target = g_getenv("DISPLAY");

    if ( target == NULL )
        return NULL;

    gint r;
    gchar *h;
    gint d;

    r = xcb_parse_display(target, &h, &d, NULL);
    if ( r == 0 )
        return NULL;
    free(h);

    if ( g_slist_find_custom(context->displays, target, g_str_equal) != NULL )
        return NULL;

    EventdNdDisplay *display;
    const xcb_query_extension_reply_t *extension_query;
    gint screen;

    display = g_new0(EventdNdDisplay, 1);

    display->source = g_xcb_source_new(NULL, target, &screen, _eventd_nd_xcb_events_callback, display, NULL);
    if ( display->source == NULL )
    {
        g_warning("Couldn't initialize X connection for '%s'", target);
        g_free(display);
        return NULL;
    }
    display->context = context;

    context->displays = g_slist_prepend(context->displays, g_strdup(target));

    display->xcb_connection = g_xcb_source_get_connection(display->source);

    display->screen = xcb_aux_get_screen(display->xcb_connection, screen);

    extension_query = xcb_get_extension_data(display->xcb_connection, &xcb_randr_id);
    if ( ! extension_query->present )
    {
        display->randr_event_base = G_MAXINT;
        g_warning("No RandR extension");
        _eventd_nd_xcb_geometry_fallback(display);
    }
    else
    {
        display->randr_event_base = extension_query->first_event;
        xcb_randr_select_input(display->xcb_connection, display->screen->root,
                XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE |
                XCB_RANDR_NOTIFY_MASK_OUTPUT_CHANGE |
                XCB_RANDR_NOTIFY_MASK_CRTC_CHANGE |
                XCB_RANDR_NOTIFY_MASK_OUTPUT_PROPERTY);
        xcb_flush(display->xcb_connection);
        _eventd_nd_xcb_randr_check_geometry(display);
    }

    extension_query = xcb_get_extension_data(display->xcb_connection, &xcb_shape_id);
    if ( ! extension_query->present )
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
    cairo_rectangle(cr, event->x, event->y, event->width, event->height);
    cairo_fill(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(cs);

    xcb_flush(self->display->xcb_connection);
}

static void
_eventd_nd_xcb_surface_button_release_event(EventdNdSurface *self)
{
    eventd_event_end(self->event, EVENTD_EVENT_END_REASON_USER_DISMISS);
}

static void
_eventd_nd_xcb_surface_shape(EventdNdSurface *self, cairo_surface_t *shape)
{
    EventdNdDisplay *display = self->display;

    if ( ! display->shape )
        return;

    gint width;
    gint height;

    width = cairo_image_surface_get_width(shape);
    height = cairo_image_surface_get_height(shape);

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
                   self->window, 0, 0, shape_id);

    xcb_free_pixmap(display->xcb_connection, shape_id);
}

static EventdNdSurface *
_eventd_nd_xcb_surface_new(EventdEvent *event, EventdNdDisplay *display, cairo_surface_t *bubble, cairo_surface_t *shape)
{
    guint32 selmask = XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
    guint32 selval[] = { 1, XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE };
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

    _eventd_nd_xcb_surface_shape(surface, shape);

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

static void
_eventd_nd_xcb_surface_update(EventdNdSurface *self, cairo_surface_t *bubble, cairo_surface_t *shape)
{
    cairo_surface_destroy(self->bubble);
    self->bubble = cairo_surface_reference(bubble);

    gint width;
    gint height;

    width = cairo_image_surface_get_width(bubble);
    height = cairo_image_surface_get_height(bubble);

    guint16 mask = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    guint32 vals[] = { width, height };

    xcb_configure_window(self->display->xcb_connection, self->window, mask, vals);
    _eventd_nd_xcb_surface_shape(self, shape);

    xcb_clear_area(self->display->xcb_connection, TRUE, self->window, 0, 0, width, height);

    xcb_flush(self->display->xcb_connection);
}

static void
_eventd_nd_xcb_surface_display(EventdNdSurface *self, gint x, gint y)
{
    if ( x < 0 )
        x += self->display->base_geometry.width;
    if ( y < 0 )
        y += self->display->base_geometry.height;
    x += self->display->base_geometry.x;
    y += self->display->base_geometry.y;

    guint16 mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
    guint32 vals[] = { x, y };

    xcb_configure_window(self->display->xcb_connection, self->window, mask, vals);
    xcb_flush(self->display->xcb_connection);
}

EVENTD_EXPORT const gchar *eventd_nd_backend_id = "eventd-nd-xcb";
EVENTD_EXPORT
void
eventd_nd_backend_get_info(EventdNdBackend *backend)
{
    backend->init = _eventd_nd_xcb_init;
    backend->uninit = _eventd_nd_xcb_uninit;

    backend->global_parse = _eventd_nd_xcb_global_parse;

    backend->display_new = _eventd_nd_xcb_display_new;
    backend->display_free = _eventd_nd_xcb_display_free;

    backend->surface_new     = _eventd_nd_xcb_surface_new;
    backend->surface_free    = _eventd_nd_xcb_surface_free;
    backend->surface_update  = _eventd_nd_xcb_surface_update;
    backend->surface_display = _eventd_nd_xcb_surface_display;
}
