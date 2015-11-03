/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2015 Quentin "Sardem FF7" Glidic
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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include <glib.h>
#include <glib-object.h>

#include <cairo.h>

#include <cairo-xcb.h>
#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <libgwater-xcb.h>
#include <xcb/randr.h>
#include <xcb/shape.h>

#include <libeventd-event.h>
#include <libeventd-helpers-config.h>

#include <eventd-nd-backend.h>

typedef enum {
    EVENTD_ND_XCB_ANCHOR_TOP,
    EVENTD_ND_XCB_ANCHOR_TOP_LEFT,
    EVENTD_ND_XCB_ANCHOR_TOP_RIGHT,
    EVENTD_ND_XCB_ANCHOR_BOTTOM,
    EVENTD_ND_XCB_ANCHOR_BOTTOM_LEFT,
    EVENTD_ND_XCB_ANCHOR_BOTTOM_RIGHT,
} EventdNdXcbCornerAnchor;

static const gchar * const _eventd_nd_xcb_corner_anchors[] = {
    [EVENTD_ND_XCB_ANCHOR_TOP]          = "top",
    [EVENTD_ND_XCB_ANCHOR_TOP_LEFT]     = "top left",
    [EVENTD_ND_XCB_ANCHOR_TOP_RIGHT]    = "top right",
    [EVENTD_ND_XCB_ANCHOR_BOTTOM]       = "bottom",
    [EVENTD_ND_XCB_ANCHOR_BOTTOM_LEFT]  = "bottom left",
    [EVENTD_ND_XCB_ANCHOR_BOTTOM_RIGHT] = "bottom right",
};
struct _EventdNdBackendContext {
    EventdNdContext *nd;
    EventdNdInterface *nd_interface;
    gchar **outputs;
    struct {
        EventdNdXcbCornerAnchor anchor;
        gboolean reverse;
        gint margin;
        gint spacing;
    } placement;
    GList *displays;
};

struct _EventdNdDisplay {
    EventdNdBackendContext *context;
    gchar *target;
    GList *link;
    GWaterXcbSource *source;
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
    GQueue *queue;
};

struct _EventdNdSurface {
    EventdEvent *event;
    EventdNdDisplay *display;
    GList *link;
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

    /* default bubble position */
    context->placement.anchor    = EVENTD_ND_XCB_ANCHOR_TOP_RIGHT;
    context->placement.margin    = 13;
    context->placement.spacing   = 13;

    return context;
}

static void
_eventd_nd_xcb_uninit(EventdNdBackendContext *context)
{
    g_list_free_full(context->displays, g_free);

    g_free(context);
}


static void
_eventd_nd_xcb_global_parse(EventdNdBackendContext *context, GKeyFile *config_file)
{
    if ( ! g_key_file_has_group(config_file, "NotificationXcb") )
        return;

    Int integer;
    guint64 enum_value;
    gboolean boolean;

    evhelpers_config_key_file_get_string_list(config_file, "NotificationXcb", "Outputs", &context->outputs, NULL);

    if ( evhelpers_config_key_file_get_enum(config_file, "NotificationXcb", "Anchor", _eventd_nd_xcb_corner_anchors, G_N_ELEMENTS(_eventd_nd_xcb_corner_anchors), &enum_value) == 0 )
        context->placement.anchor = enum_value;

    if ( evhelpers_config_key_file_get_boolean(config_file, "NotificationXcb", "OldestFirst", &boolean) == 0 )
        context->placement.reverse = boolean;

    if ( evhelpers_config_key_file_get_int(config_file, "NotificationXcb", "Margin", &integer) == 0 )
        context->placement.margin = integer.value;

    if ( evhelpers_config_key_file_get_int(config_file, "NotificationXcb", "Spacing", &integer) == 0 )
        context->placement.spacing = integer.value;
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

static void _eventd_nd_xcb_update_surfaces(EventdNdDisplay *display);

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

    _eventd_nd_xcb_update_surfaces(display);
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
        display->context->nd_interface->remove_display(display->context->nd, display->target);
        return FALSE;
    }

    gint type = event->response_type & ~0x80;

    switch ( type - display->randr_event_base )
    {
    case XCB_RANDR_SCREEN_CHANGE_NOTIFY:
        _eventd_nd_xcb_randr_check_geometry(display);
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

static const gchar *
_eventd_nd_xcb_default_target(EventdNdBackendContext *context)
{
    return g_getenv("DISPLAY");
}

static EventdNdDisplay *
_eventd_nd_xcb_display_new(EventdNdBackendContext *context, const gchar *target)
{
    g_return_val_if_fail(target != NULL, NULL);

    gint r;
    gchar *h;
    gint d;

    r = xcb_parse_display(target, &h, &d, NULL);
    if ( r == 0 )
        return NULL;
    free(h);

    if ( g_list_find_custom(context->displays, target, (GCompareFunc) g_strcmp0) != NULL )
        return NULL;

    EventdNdDisplay *display;
    const xcb_query_extension_reply_t *extension_query;
    gint screen;

    display = g_new0(EventdNdDisplay, 1);

    display->source = g_water_xcb_source_new(NULL, target, &screen, _eventd_nd_xcb_events_callback, display, NULL);
    if ( display->source == NULL )
    {
        g_warning("Couldn't initialize X connection for '%s'", target);
        g_free(display);
        return NULL;
    }
    display->context = context;

    display->target = g_strdup(target);
    context->displays = g_list_prepend(context->displays, display->target);
    display->link = context->displays;

    display->xcb_connection = g_water_xcb_source_get_connection(display->source);

    display->screen = xcb_aux_get_screen(display->xcb_connection, screen);

    display->bubbles = g_hash_table_new(NULL, NULL);
    display->queue = g_queue_new();

    extension_query = xcb_get_extension_data(display->xcb_connection, &xcb_shape_id);
    if ( ! extension_query->present )
        g_warning("No Shape extension");
    else
        display->shape = TRUE;

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

    return display;
}

static void
_eventd_nd_xcb_display_free(EventdNdDisplay *display)
{
    g_hash_table_unref(display->bubbles);
    g_water_xcb_source_unref(display->source);
    display->context->displays = g_list_delete_link(display->context->displays, display->link);
    g_free(display);
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
_eventd_nd_xcb_surface_shape(EventdNdSurface *self, cairo_surface_t *bubble)
{
    EventdNdDisplay *display = self->display;

    if ( ! display->shape )
        return;

    gint width;
    gint height;

    width = cairo_image_surface_get_width(bubble);
    height = cairo_image_surface_get_height(bubble);

    xcb_pixmap_t shape_id;
    cairo_surface_t *shape;
    cairo_t *cr;

    shape_id = xcb_generate_id(display->xcb_connection);
    xcb_create_pixmap(display->xcb_connection, 1,
                      shape_id, display->screen->root,
                      width, height);

    shape = cairo_xcb_surface_create_for_bitmap(display->xcb_connection, display->screen, shape_id, width, height);
    cr = cairo_create(shape);

    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(cr, bubble, 0, 0);
    cairo_paint(cr);

    cairo_destroy(cr);
    cairo_surface_destroy(shape);

    xcb_shape_mask(display->xcb_connection,
                   XCB_SHAPE_SO_INTERSECT, XCB_SHAPE_SK_BOUNDING,
                   self->window, 0, 0, shape_id);

    xcb_free_pixmap(display->xcb_connection, shape_id);
}

static void
_eventd_nd_xcb_update_surfaces(EventdNdDisplay *display)
{
    EventdNdBackendContext *context = display->context;
    GList *surface_;
    EventdNdSurface *surface;

    gboolean right, center, bottom;
    right = ( context->placement.anchor == EVENTD_ND_XCB_ANCHOR_TOP_RIGHT ) || ( context->placement.anchor == EVENTD_ND_XCB_ANCHOR_BOTTOM_RIGHT );
    center = ( context->placement.anchor == EVENTD_ND_XCB_ANCHOR_TOP ) || ( context->placement.anchor == EVENTD_ND_XCB_ANCHOR_BOTTOM );
    bottom = ( context->placement.anchor == EVENTD_ND_XCB_ANCHOR_BOTTOM_LEFT ) || ( context->placement.anchor == EVENTD_ND_XCB_ANCHOR_BOTTOM ) || ( context->placement.anchor == EVENTD_ND_XCB_ANCHOR_BOTTOM_RIGHT );

    guint16 mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
    guint32 vals[] = { 0, 0 };
    gint x, y;
    x = context->placement.margin;
    y = context->placement.margin;
    if ( center )
        x = display->base_geometry.width;
    else if ( right )
        x = display->base_geometry.width - x;
    if ( bottom )
        y = display->base_geometry.height - y;
    for ( surface_ = g_queue_peek_head_link(display->queue) ; surface_ != NULL ; surface_ = g_list_next(surface_) )
    {
        gint width;
        gint height;
        surface = surface_->data;

        width = cairo_image_surface_get_width(surface->bubble);
        height = cairo_image_surface_get_height(surface->bubble);
        if ( bottom )
            y -= height;

        vals[0] = display->base_geometry.x + ( center ? ( ( x / 2 ) - ( width / 2 ) ) : right ? ( x - width ) : x );
        vals[1] = display->base_geometry.y + y;
        xcb_configure_window(display->xcb_connection, surface->window, mask, vals);

        if ( bottom )
            y -= context->placement.spacing;
        else
            y += height + context->placement.spacing;
    }

    xcb_flush(display->xcb_connection);
}

static EventdNdSurface *
_eventd_nd_xcb_surface_new(EventdEvent *event, EventdNdDisplay *display, cairo_surface_t *bubble)
{
    EventdNdBackendContext *context = display->context;
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

    _eventd_nd_xcb_surface_shape(surface, bubble);

    xcb_map_window(surface->display->xcb_connection, surface->window);

    g_hash_table_insert(surface->display->bubbles, GUINT_TO_POINTER(surface->window), surface);
    if ( context->placement.reverse )
    {
        g_queue_push_tail(display->queue, surface);
        surface->link = g_queue_peek_tail_link(display->queue);
    }
    else
    {
        g_queue_push_head(display->queue, surface);
        surface->link = g_queue_peek_head_link(display->queue);
    }

    _eventd_nd_xcb_update_surfaces(display);

    return surface;
}

static void
_eventd_nd_xcb_surface_free(EventdNdSurface *surface)
{
    if ( surface == NULL )
        return;

    EventdNdDisplay *display = surface->display;

    g_queue_delete_link(display->queue, surface->link);
    g_hash_table_remove(display->bubbles, GUINT_TO_POINTER(surface->window));

    if ( ! g_source_is_destroyed((GSource *)display->source) )
    {
        xcb_unmap_window(display->xcb_connection, surface->window);
        _eventd_nd_xcb_update_surfaces(display);
    }

    cairo_surface_destroy(surface->bubble);

    g_object_unref(surface->event);

    g_free(surface);

}

static void
_eventd_nd_xcb_surface_update(EventdNdSurface *self, cairo_surface_t *bubble)
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
    _eventd_nd_xcb_surface_shape(self, bubble);

    xcb_clear_area(self->display->xcb_connection, TRUE, self->window, 0, 0, width, height);

    _eventd_nd_xcb_update_surfaces(self->display);
}

EVENTD_EXPORT const gchar *eventd_nd_backend_id = "nd-xcb";
EVENTD_EXPORT
void
eventd_nd_backend_get_info(EventdNdBackend *backend)
{
    backend->init = _eventd_nd_xcb_init;
    backend->uninit = _eventd_nd_xcb_uninit;

    backend->global_parse = _eventd_nd_xcb_global_parse;

    backend->default_target = _eventd_nd_xcb_default_target;
    backend->display_new    = _eventd_nd_xcb_display_new;
    backend->display_free   = _eventd_nd_xcb_display_free;

    backend->surface_new     = _eventd_nd_xcb_surface_new;
    backend->surface_free    = _eventd_nd_xcb_surface_free;
    backend->surface_update  = _eventd_nd_xcb_surface_update;
}
