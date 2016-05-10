/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2016 Quentin "Sardem FF7" Glidic
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
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

#include "backend.h"

typedef enum {
    EVENTD_ND_XCB_FOLLOW_FOCUS_NONE,
    EVENTD_ND_XCB_FOLLOW_FOCUS_KEYBOARD,
    EVENTD_ND_XCB_FOLLOW_FOCUS_MOUSE,
} EventdNdXcbFollowFocus;

static const gchar * const _eventd_nd_xcb_follow_focus[] = {
    [EVENTD_ND_XCB_FOLLOW_FOCUS_NONE]     = "none",
    [EVENTD_ND_XCB_FOLLOW_FOCUS_KEYBOARD] = "keyboard",
    [EVENTD_ND_XCB_FOLLOW_FOCUS_MOUSE]    = "mouse",
};

struct _EventdNdBackendContext {
    EventdNdInterface *nd;
    EventdNdXcbFollowFocus follow_focus;
    gchar **outputs;
    GWaterXcbSource *source;
    xcb_connection_t *xcb_connection;
    gint screen_number;
    xcb_screen_t *screen;
    xcb_visualtype_t *visual;
    struct {
        gint x;
        gint y;
        gint w;
        gint h;
    } geometry;
    gboolean randr;
    gint randr_event_base;
    gboolean shape;
    GHashTable *bubbles;
};

struct _EventdNdSurface {
    EventdNdNotification *notification;
    EventdNdBackendContext *context;
    xcb_window_t window;
    gint width;
    gint height;
    cairo_surface_t *bubble;
    gboolean mapped;
};

static EventdNdBackendContext *
_eventd_nd_xcb_init(EventdNdInterface *nd)
{
    EventdNdBackendContext *self;

    self = g_new0(EventdNdBackendContext, 1);

    self->nd = nd;

    return self;
}

static void
_eventd_nd_xcb_uninit(EventdNdBackendContext *self)
{
    g_free(self);
}


static void
_eventd_nd_xcb_global_parse(EventdNdBackendContext *self, GKeyFile *config_file)
{
    if ( ! g_key_file_has_group(config_file, "NotificationXcb") )
        return;

    guint64 enum_value;

    if ( evhelpers_config_key_file_get_enum_with_default(config_file, "NotificationXcb", "FollowFocus", _eventd_nd_xcb_follow_focus, G_N_ELEMENTS(_eventd_nd_xcb_follow_focus), EVENTD_ND_XCB_FOLLOW_FOCUS_NONE, &enum_value) == 0 )
        self->follow_focus = enum_value;
    evhelpers_config_key_file_get_string_list(config_file, "NotificationXcb", "Outputs", &self->outputs, NULL);
}

static void
_eventd_nd_xcb_config_reset(EventdNdBackendContext *self)
{
    self->follow_focus = FALSE;
    g_strfreev(self->outputs);
    self->outputs = NULL;
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
_eventd_nd_xcb_geometry_fallback(EventdNdBackendContext *self)
{
    self->geometry.x = 0;
    self->geometry.y = 0;
    self->geometry.w = self->screen->width_in_pixels;
    self->geometry.h = self->screen->height_in_pixels;
}

static gboolean
_eventd_nd_xcb_randr_check_primary(EventdNdBackendContext *self)
{
    xcb_randr_get_output_primary_cookie_t pcookie;
    xcb_randr_get_output_primary_reply_t *primary;

    pcookie = xcb_randr_get_output_primary(self->xcb_connection, self->screen->root);
    if ( ( primary = xcb_randr_get_output_primary_reply(self->xcb_connection, pcookie, NULL) ) == NULL )
        return FALSE;

    gboolean found = FALSE;

    xcb_randr_get_output_info_cookie_t ocookie;
    xcb_randr_get_output_info_reply_t *output;

    ocookie = xcb_randr_get_output_info(self->xcb_connection, primary->output, 0);
    if ( ( output = xcb_randr_get_output_info_reply(self->xcb_connection, ocookie, NULL) ) != NULL )
    {

        xcb_randr_get_crtc_info_cookie_t ccookie;
        xcb_randr_get_crtc_info_reply_t *crtc;

        ccookie = xcb_randr_get_crtc_info(self->xcb_connection, output->crtc, output->timestamp);
        if ( ( crtc = xcb_randr_get_crtc_info_reply(self->xcb_connection, ccookie, NULL) ) != NULL )
        {
            found = TRUE;

            self->geometry.x = crtc->x;
            self->geometry.y = crtc->y;
            self->geometry.w = crtc->width;
            self->geometry.h = crtc->height;

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
_eventd_nd_xcb_randr_check_config_outputs(EventdNdBackendContext *self, EventdNdXcbRandrOutput *output)
{
    gchar **config_output;
    for ( config_output = self->outputs ; *config_output != NULL ; ++config_output )
    {
        for ( ; output->output != NULL ; ++output )
        {
            if ( g_ascii_strncasecmp(*config_output, (const gchar *)xcb_randr_get_output_info_name(output->output), xcb_randr_get_output_info_name_length(output->output)) != 0 )
                continue;
            self->geometry.x = output->crtc->x;
            self->geometry.y = output->crtc->y;
            self->geometry.w = output->crtc->width;
            self->geometry.h = output->crtc->height;

            return TRUE;
        }
    }
    return FALSE;
}

static gboolean
_eventd_nd_xcb_randr_check_focused(EventdNdBackendContext *self, EventdNdXcbRandrOutput *output)
{
    gint x, y;
    switch ( self->follow_focus )
    {
    case EVENTD_ND_XCB_FOLLOW_FOCUS_MOUSE:
    {
        xcb_query_pointer_cookie_t c;
        xcb_query_pointer_reply_t *r;
        c = xcb_query_pointer(self->xcb_connection, self->screen->root);
        r = xcb_query_pointer_reply(self->xcb_connection, c, NULL);
        if ( r == NULL )
            return FALSE;

        x = r->root_x;
        y = r->root_y;
        free(r);
    }
    break;
    case EVENTD_ND_XCB_FOLLOW_FOCUS_KEYBOARD:
    {
        xcb_intern_atom_cookie_t ac;
        xcb_intern_atom_reply_t *ar;
        ac = xcb_intern_atom(self->xcb_connection, FALSE, strlen("_NET_ACTIVE_WINDOW"), "_NET_ACTIVE_WINDOW");
        ar = xcb_intern_atom_reply(self->xcb_connection, ac, NULL);
        if ( ar == NULL )
            return FALSE;

        xcb_atom_t a;
        a = ar->atom;
        free(ar);

        xcb_get_property_cookie_t wc;
        xcb_get_property_reply_t *wr;
        wc = xcb_get_property(self->xcb_connection, FALSE, self->screen->root, a, XCB_ATOM_WINDOW, 0, sizeof(xcb_window_t));
        wr = xcb_get_property_reply(self->xcb_connection, wc, NULL);
        if ( wr == NULL )
            return FALSE;

        xcb_window_t w;
        w = *((xcb_window_t *) xcb_get_property_value(wr));
        free(wr);

        xcb_translate_coordinates_cookie_t cc;
        xcb_translate_coordinates_reply_t *cr;
        cc = xcb_translate_coordinates(self->xcb_connection, w, self->screen->root, 0, 0);
        xcb_generic_error_t *e;
        cr = xcb_translate_coordinates_reply(self->xcb_connection, cc, &e);
        if ( cr == NULL )
        {
            g_debug("error %d", e->error_code);
            return FALSE;
        }

        x = cr->dst_x;
        y = cr->dst_y;
        free(cr);
    }
    break;
    default:
        g_return_val_if_reached(FALSE);
    }

    for ( ; output->output != NULL ; ++output )
    {
        if ( ! ( ( x >= output->crtc->x && x <= ( output->crtc->x + output->crtc->width ) ) && ( y >= output->crtc->y && y <= ( output->crtc->y + output->crtc->height ) ) ) )
            continue;
        self->geometry.x = output->crtc->x;
        self->geometry.y = output->crtc->y;
        self->geometry.w = output->crtc->width;
        self->geometry.h = output->crtc->height;

        return TRUE;
    }
    return FALSE;
}

static gboolean
_eventd_nd_xcb_randr_check_outputs(EventdNdBackendContext *self)
{
    if ( ( self->follow_focus == EVENTD_ND_XCB_FOLLOW_FOCUS_NONE ) && ( self->outputs == NULL ) )
        return FALSE;

    xcb_randr_get_screen_resources_current_cookie_t rcookie;
    xcb_randr_get_screen_resources_current_reply_t *ressources;

    rcookie = xcb_randr_get_screen_resources_current(self->xcb_connection, self->screen->root);
    if ( ( ressources = xcb_randr_get_screen_resources_current_reply(self->xcb_connection, rcookie, NULL) ) == NULL )
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

    EventdNdXcbRandrOutput outputs[length + 1];
    EventdNdXcbRandrOutput *output;

    output = outputs;

    for ( i = 0 ; i < length ; ++i )
    {
        xcb_randr_get_output_info_cookie_t ocookie;

        ocookie = xcb_randr_get_output_info(self->xcb_connection, randr_outputs[i], cts);
        if ( ( output->output = xcb_randr_get_output_info_reply(self->xcb_connection, ocookie, NULL) ) == NULL )
            continue;

        xcb_randr_get_crtc_info_cookie_t ccookie;

        ccookie = xcb_randr_get_crtc_info(self->xcb_connection, output->output->crtc, cts);
        if ( ( output->crtc = xcb_randr_get_crtc_info_reply(self->xcb_connection, ccookie, NULL) ) == NULL )
            free(output->output);
        else
            ++output;
    }
    output->output = NULL;

    gboolean found;

    if ( self->follow_focus != EVENTD_ND_XCB_FOLLOW_FOCUS_NONE )
        found = _eventd_nd_xcb_randr_check_focused(self, outputs);
    else
        found = _eventd_nd_xcb_randr_check_config_outputs(self, outputs);

    for ( output = outputs ; output->output != NULL ; ++output )
    {
        free(output->crtc);
        free(output->output);
    }

    return found;
}

static void
_eventd_nd_xcb_check_geometry(EventdNdBackendContext *self)
{
    gint x, y, w, h;
    x = self->geometry.x;
    y = self->geometry.y;
    w = self->geometry.w;
    h = self->geometry.h;

    gboolean found = FALSE;
    if ( self->randr )
    {
        found = _eventd_nd_xcb_randr_check_outputs(self);
        if ( ! found )
            found = _eventd_nd_xcb_randr_check_primary(self);
    }
    if ( ! found )
        _eventd_nd_xcb_geometry_fallback(self);

    if ( ( x == self->geometry.x ) && ( y == self->geometry.y ) && ( w == self->geometry.w ) && ( h == self->geometry.h ) )
        return;

    self->nd->geometry_update(self->nd->context, self->geometry.w, self->geometry.h);
}

static void _eventd_nd_xcb_surface_expose_event(EventdNdSurface *self, xcb_expose_event_t *event);
static void _eventd_nd_xcb_surface_button_release_event(EventdNdSurface *self);


static gboolean
_eventd_nd_xcb_events_callback(xcb_generic_event_t *event, gpointer user_data)
{
    EventdNdBackendContext *self = user_data;
    EventdNdSurface *surface;

    if ( event == NULL )
    {
        self->nd->backend_stop(self->nd->context);
        return FALSE;
    }

    gint type = event->response_type & ~0x80;

    /* RandR events */
    if ( self->randr )
    switch ( type - self->randr_event_base )
    {
    case XCB_RANDR_SCREEN_CHANGE_NOTIFY:
        _eventd_nd_xcb_check_geometry(self);
        return TRUE;
    case XCB_RANDR_NOTIFY:
        return TRUE;
    default:
    break;
    }

    /* Core events */
    switch ( type )
    {
    case XCB_EXPOSE:
    {
        xcb_expose_event_t *e = (xcb_expose_event_t *)event;

        surface = g_hash_table_lookup(self->bubbles, GUINT_TO_POINTER(e->window));
        if ( surface != NULL )
            _eventd_nd_xcb_surface_expose_event(surface, e);
    }
    break;
    case XCB_BUTTON_RELEASE:
    {
        xcb_button_release_event_t *e = (xcb_button_release_event_t *)event;

        surface = g_hash_table_lookup(self->bubbles, GUINT_TO_POINTER(e->event));
        if ( surface != NULL )
            _eventd_nd_xcb_surface_button_release_event(surface);
    }
    break;
    case XCB_PROPERTY_NOTIFY:
    {
        xcb_property_notify_event_t *e = (xcb_property_notify_event_t *)event;
        if ( e->window == self->screen->root )
            _eventd_nd_xcb_check_geometry(self);
    }
    break;
    default:
    break;
    }

    return TRUE;
}

static gboolean
_eventd_nd_xcb_start(EventdNdBackendContext *self, const gchar *target)
{
    gint r;
    gchar *h;
    gint d;

    r = xcb_parse_display(target, &h, &d, NULL);
    if ( r == 0 )
        return FALSE;
    free(h);

    const xcb_query_extension_reply_t *extension_query;
    gint screen;

    self->source = g_water_xcb_source_new(NULL, target, &screen, _eventd_nd_xcb_events_callback, self, NULL);
    if ( self->source == NULL )
    {
        g_warning("Couldn't initialize X connection for '%s'", target);
        return FALSE;
    }

    self->xcb_connection = g_water_xcb_source_get_connection(self->source);
    self->screen_number = screen;
    self->screen = xcb_aux_get_screen(self->xcb_connection, screen);
    self->visual = get_root_visual_type(self->screen);

    self->bubbles = g_hash_table_new(NULL, NULL);

    if ( self->follow_focus != EVENTD_ND_XCB_FOLLOW_FOCUS_NONE )
    {
        guint32 mask[] = { XCB_EVENT_MASK_PROPERTY_CHANGE };
        xcb_change_window_attributes(self->xcb_connection, self->screen->root, XCB_CW_EVENT_MASK, mask);
    }

    extension_query = xcb_get_extension_data(self->xcb_connection, &xcb_shape_id);
    if ( ! extension_query->present )
        g_warning("No Shape extension");
    else
        self->shape = TRUE;

    extension_query = xcb_get_extension_data(self->xcb_connection, &xcb_randr_id);
    if ( ! extension_query->present )
    {
        g_warning("No RandR extension");
    }
    else
    {
        self->randr = TRUE;
        self->randr_event_base = extension_query->first_event;
        xcb_randr_select_input(self->xcb_connection, self->screen->root,
                XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE |
                XCB_RANDR_NOTIFY_MASK_OUTPUT_CHANGE |
                XCB_RANDR_NOTIFY_MASK_CRTC_CHANGE |
                XCB_RANDR_NOTIFY_MASK_OUTPUT_PROPERTY);
    }

    xcb_flush(self->xcb_connection);
    _eventd_nd_xcb_check_geometry(self);

    return TRUE;
}

static void
_eventd_nd_xcb_stop(EventdNdBackendContext *self)
{
    g_hash_table_unref(self->bubbles);
    g_water_xcb_source_free(self->source);
    self->bubbles = NULL;
    self->source = NULL;
}

static void
_eventd_nd_xcb_surface_expose_event(EventdNdSurface *self, xcb_expose_event_t *event)
{
    cairo_surface_t *cs;
    cairo_t *cr;

    cs = cairo_xcb_surface_create(self->context->xcb_connection, self->window, self->context->visual, self->width, self->height);
    cr = cairo_create(cs);
    cairo_set_source_surface(cr, self->bubble, 0, 0);
    cairo_rectangle(cr, event->x, event->y, event->width, event->height);
    cairo_fill(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(cs);

    xcb_flush(self->context->xcb_connection);
}

static void
_eventd_nd_xcb_surface_button_release_event(EventdNdSurface *self)
{
    self->context->nd->notification_dismiss(self->notification);
}

static void
_eventd_nd_xcb_surface_draw(EventdNdSurface *self)
{
    EventdNdBackendContext *context = self->context;

    cairo_t *cr;
    cr = cairo_create(self->bubble);
    self->context->nd->notification_draw(self->notification, cr, FALSE);
    cairo_destroy(cr);

    if ( ! context->shape )
        return;

    xcb_pixmap_t shape_id;
    cairo_surface_t *shape;

    shape_id = xcb_generate_id(context->xcb_connection);
    xcb_create_pixmap(context->xcb_connection, 1,
                      shape_id, context->screen->root,
                      self->width, self->height);

    shape = cairo_xcb_surface_create_for_bitmap(context->xcb_connection, context->screen, shape_id, self->width, self->height);
    cr = cairo_create(shape);

    cairo_set_source_rgba(cr, 1, 1, 1, 1);
    self->context->nd->notification_shape(self->notification, cr);
    cairo_fill(cr);

    cairo_destroy(cr);
    cairo_surface_destroy(shape);

    xcb_shape_mask(context->xcb_connection,
                   XCB_SHAPE_SO_INTERSECT, XCB_SHAPE_SK_BOUNDING,
                   self->window, 0, 0, shape_id);

    xcb_free_pixmap(context->xcb_connection, shape_id);
}

static EventdNdSurface *
_eventd_nd_xcb_surface_new(EventdNdBackendContext *context, EventdNdNotification *notification, gint width, gint height)
{
    guint32 selmask = XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
    guint32 selval[] = { 1, XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE };
    EventdNdSurface *self;

    self = g_new0(EventdNdSurface, 1);

    self->notification = notification;

    self->context = context;
    self->width = width;
    self->height = height;

    self->window = xcb_generate_id(context->xcb_connection);
    xcb_create_window(context->xcb_connection,
                                       context->screen->root_depth,   /* depth         */
                                       self->window,
                                       context->screen->root,         /* parent window */
                                       0, 0,                          /* x, y          */
                                       width, height,                 /* width, height */
                                       0,                             /* border_width  */
                                       XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class         */
                                       context->visual->visual_id,    /* visual        */
                                       selmask, selval);              /* masks         */

    self->bubble = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);

    _eventd_nd_xcb_surface_draw(self);

    g_hash_table_insert(context->bubbles, GUINT_TO_POINTER(self->window), self);

    return self;
}

static void
_eventd_nd_xcb_surface_update(EventdNdSurface *self, gint width, gint height)
{
    self->width = width;
    self->height = height;
    cairo_surface_destroy(self->bubble);
    self->bubble = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);

    guint16 mask = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    guint32 vals[] = { width, height };

    xcb_configure_window(self->context->xcb_connection, self->window, mask, vals);

    _eventd_nd_xcb_surface_draw(self);

    xcb_clear_area(self->context->xcb_connection, TRUE, self->window, 0, 0, 0, 0);
}

static void
_eventd_nd_xcb_surface_free(EventdNdSurface *self)
{
    if ( self == NULL )
        return;

    EventdNdBackendContext *context = self->context;

    g_hash_table_remove(context->bubbles, GUINT_TO_POINTER(self->window));

    if ( ! g_source_is_destroyed((GSource *)context->source) )
        xcb_unmap_window(context->xcb_connection, self->window);

    cairo_surface_destroy(self->bubble);

    g_free(self);
}

static void
_eventd_nd_xcb_move_surface(EventdNdSurface *self, gint x, gint y, gpointer data)
{
    x += self->context->geometry.x;
    y += self->context->geometry.y;

    guint16 mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
    guint32 vals[] = { x, y };
    xcb_configure_window(self->context->xcb_connection, self->window, mask, vals);
    xcb_clear_area(self->context->xcb_connection, TRUE, self->window, 0, 0, 0, 0);

    if ( ! self->mapped )
    {
        xcb_map_window(self->context->xcb_connection, self->window);
        self->mapped = TRUE;
    }
}

static void
_eventd_nd_xcb_move_end(EventdNdBackendContext *context, gpointer data)
{
    xcb_flush(context->xcb_connection);
}

EVENTD_EXPORT
void
eventd_nd_backend_get_info(EventdNdBackend *backend)
{
    backend->init = _eventd_nd_xcb_init;
    backend->uninit = _eventd_nd_xcb_uninit;

    backend->global_parse = _eventd_nd_xcb_global_parse;
    backend->config_reset = _eventd_nd_xcb_config_reset;

    backend->start = _eventd_nd_xcb_start;
    backend->stop  = _eventd_nd_xcb_stop;

    backend->surface_new    = _eventd_nd_xcb_surface_new;
    backend->surface_update = _eventd_nd_xcb_surface_update;
    backend->surface_free   = _eventd_nd_xcb_surface_free;

    backend->move_surface = _eventd_nd_xcb_move_surface;
    backend->move_end     = _eventd_nd_xcb_move_end;
}
