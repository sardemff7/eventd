/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2017 Quentin "Sardem FF7" Glidic
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

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <glib-object.h>

#include <cairo.h>

#include <cairo-xcb.h>
#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <libgwater-xcb.h>
#include <xcb/randr.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xfixes.h>
#include <xcb/shape.h>

#include "libeventd-event.h"
#include "libeventd-helpers-config.h"

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
    guint8 depth;
    xcb_visualtype_t *visual;
    xcb_colormap_t map;
    struct {
        gint x;
        gint y;
        gint w;
        gint h;
        gint s;
    } geometry;
    gboolean randr;
    gboolean compositing;
    gboolean custom_map;
    gboolean xfixes;
    gboolean shape;
    xcb_ewmh_connection_t ewmh;
    gint randr_event_base;
    gint xfixes_event_base;
    GHashTable *bubbles;
};

struct _EventdNdSurface {
    EventdNdNotification *notification;
    EventdNdBackendContext *context;
    xcb_window_t window;
    gint width;
    gint height;
    cairo_surface_t *surface;
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
    gchar **outputs;

    if ( evhelpers_config_key_file_get_enum_with_default(config_file, "NotificationXcb", "FollowFocus", _eventd_nd_xcb_follow_focus, G_N_ELEMENTS(_eventd_nd_xcb_follow_focus), EVENTD_ND_XCB_FOLLOW_FOCUS_NONE, &enum_value) == 0 )
        self->follow_focus = enum_value;
    if ( evhelpers_config_key_file_get_string_list(config_file, "NotificationXcb", "Outputs", &outputs, NULL) == 0 )
    {
        g_strfreev(self->outputs);
        self->outputs = outputs;
    }
}

static void
_eventd_nd_xcb_config_reset(EventdNdBackendContext *self)
{
    self->follow_focus = FALSE;
    g_strfreev(self->outputs);
    self->outputs = NULL;
}

static gboolean
_eventd_nd_xcb_get_colormap(EventdNdBackendContext *self)
{
    gboolean ret = FALSE;

    self->visual = xcb_aux_find_visual_by_attrs(self->screen, XCB_VISUAL_CLASS_DIRECT_COLOR, 32);
    if ( self->visual == NULL )
        self->visual = xcb_aux_find_visual_by_attrs(self->screen, XCB_VISUAL_CLASS_TRUE_COLOR, 32);

    if ( self->visual != NULL )
    {
        xcb_void_cookie_t c;
        xcb_generic_error_t *e;
        self->map = xcb_generate_id(self->xcb_connection);
        c = xcb_create_colormap_checked(self->xcb_connection, XCB_COLORMAP_ALLOC_NONE, self->map, self->screen->root, self->visual->visual_id);
        e = xcb_request_check(self->xcb_connection, c);
        if ( e == NULL )
            ret = TRUE;
        else
        {
            xcb_free_colormap(self->xcb_connection, self->map);
            free(e);
        }
    }

    if ( ! ret )
    {
        self->visual = xcb_aux_find_visual_by_id(self->screen, self->screen->root_visual);
        self->map = self->screen->default_colormap;
    }

    self->depth = xcb_aux_get_depth_of_visual(self->screen, self->visual->visual_id);
    return ret;
}

static void
_eventd_nd_xcb_geometry_fallback(EventdNdBackendContext *self)
{
    self->geometry.x = 0;
    self->geometry.y = 0;
    self->geometry.w = self->screen->width_in_pixels;
    self->geometry.h = self->screen->height_in_pixels;
    self->geometry.s = 1;
}

static void
_eventd_nd_xcb_randr_set_output(EventdNdBackendContext *self, xcb_randr_get_output_info_reply_t *output, xcb_randr_get_crtc_info_reply_t *crtc)
{
    self->geometry.x = crtc->x;
    self->geometry.y = crtc->y;
    self->geometry.w = crtc->width;
    self->geometry.h = crtc->height;
    self->geometry.s = _eventd_nd_compute_scale(self->geometry.w, self->geometry.h, output->mm_width, output->mm_height);
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

            _eventd_nd_xcb_randr_set_output(self, output, crtc);

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

            _eventd_nd_xcb_randr_set_output(self, output->output, output->crtc);

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
        xcb_get_property_cookie_t wc;
        xcb_get_property_reply_t *wr;
        wc = xcb_get_property(self->xcb_connection, FALSE, self->screen->root, self->ewmh._NET_ACTIVE_WINDOW, XCB_ATOM_WINDOW, 0, sizeof(xcb_window_t));
        wr = xcb_get_property_reply(self->xcb_connection, wc, NULL);
        if ( wr == NULL )
            return FALSE;

        xcb_window_t w;
        w = *((xcb_window_t *) xcb_get_property_value(wr));
        free(wr);

        xcb_translate_coordinates_cookie_t cc;
        xcb_translate_coordinates_reply_t *cr;
        cc = xcb_translate_coordinates(self->xcb_connection, w, self->screen->root, 0, 0);
        cr = xcb_translate_coordinates_reply(self->xcb_connection, cc, NULL);
        if ( cr == NULL )
            return FALSE;

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

        _eventd_nd_xcb_randr_set_output(self, output->output, output->crtc);

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
    gint x, y, w, h, s;
    x = self->geometry.x;
    y = self->geometry.y;
    w = self->geometry.w;
    h = self->geometry.h;
    s = self->geometry.s;

    gboolean found = FALSE;
    if ( self->randr )
    {
        found = _eventd_nd_xcb_randr_check_outputs(self);
        if ( ! found )
            found = _eventd_nd_xcb_randr_check_primary(self);
    }
    if ( ! found )
        _eventd_nd_xcb_geometry_fallback(self);

    if ( ( x == self->geometry.x ) && ( y == self->geometry.y ) && ( w == self->geometry.w ) && ( h == self->geometry.h ) && ( s == self->geometry.s ) )
        return;

    self->nd->geometry_update(self->nd->context, self->geometry.w, self->geometry.h, self->geometry.s);
}

static void _eventd_nd_xcb_surface_button_release_event(EventdNdSurface *self);
static void _eventd_nd_xcb_surface_draw(EventdNdSurface *self);


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

    /* XFixes events */
    if ( self->xfixes )
    switch ( type - self->xfixes_event_base )
    {
    case XCB_XFIXES_SELECTION_NOTIFY:
    {
        xcb_xfixes_selection_notify_event_t *e = (xcb_xfixes_selection_notify_event_t *)event;
        if ( e->selection == self->ewmh._NET_WM_CM_Sn[self->screen_number] )
        {
            gboolean compositing = ( e->owner != XCB_WINDOW_NONE );
            if ( self->compositing != compositing )
            {
                xcb_rectangle_t rectangles[] = { { 0, 0, 0, 0 } };
                self->compositing = compositing;
                GHashTableIter iter;
                g_hash_table_iter_init(&iter, self->bubbles);
                while ( g_hash_table_iter_next(&iter, NULL, (gpointer *) &surface) )
                {
                    if ( compositing && self->shape )
                    {
                        rectangles[0].width = surface->width;
                        rectangles[0].height = surface->height;
                        xcb_shape_rectangles(self->xcb_connection, XCB_SHAPE_SO_UNION, XCB_SHAPE_SK_BOUNDING, 0, surface->window, 0, 0, 1, rectangles);
                    }
                    _eventd_nd_xcb_surface_draw(surface);
                }
            }
        }

        return TRUE;
    }
    break;
    }

    /* Core events */
    switch ( type )
    {
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
        if ( e->window != self->screen->root )
            break;
        if ( ( self->follow_focus != EVENTD_ND_XCB_FOLLOW_FOCUS_NONE ) && ( e->atom == self->ewmh._NET_ACTIVE_WINDOW ) )
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

    self->bubbles = g_hash_table_new(NULL, NULL);

    if ( self->follow_focus != EVENTD_ND_XCB_FOLLOW_FOCUS_NONE )
    {
        guint32 mask[] = { XCB_EVENT_MASK_PROPERTY_CHANGE };
        xcb_change_window_attributes(self->xcb_connection, self->screen->root, XCB_CW_EVENT_MASK, mask);
    }

    xcb_intern_atom_cookie_t *ac;
    ac = xcb_ewmh_init_atoms(self->xcb_connection, &self->ewmh);
    xcb_ewmh_init_atoms_replies(&self->ewmh, ac, NULL);

    extension_query = xcb_get_extension_data(self->xcb_connection, &xcb_randr_id);
    if ( ! extension_query->present )
        g_warning("No RandR extension");
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

    self->custom_map = _eventd_nd_xcb_get_colormap(self);

    if ( self->custom_map )
    {
        /* We have a 32bit color map, try to support compositing */
        xcb_get_selection_owner_cookie_t oc;
        xcb_window_t owner;
        oc = xcb_ewmh_get_wm_cm_owner(&self->ewmh, self->screen_number);
        self->compositing = xcb_ewmh_get_wm_cm_owner_reply(&self->ewmh, oc, &owner, NULL) && ( owner != XCB_WINDOW_NONE );

        extension_query = xcb_get_extension_data(self->xcb_connection, &xcb_xfixes_id);
        if ( ! extension_query->present )
            g_warning("No XFixes extension");
        else
        {
            xcb_xfixes_query_version_cookie_t vc;
            xcb_xfixes_query_version_reply_t *r;
            vc = xcb_xfixes_query_version(self->xcb_connection, XCB_XFIXES_MAJOR_VERSION, XCB_XFIXES_MINOR_VERSION);
            r = xcb_xfixes_query_version_reply(self->xcb_connection, vc, NULL);
            if ( r == NULL )
                g_warning("Cannot get XFixes version");
            else
            {
                self->xfixes = TRUE;
                self->xfixes_event_base = extension_query->first_event;
                xcb_xfixes_select_selection_input_checked(self->xcb_connection, self->screen->root,
                    self->ewmh._NET_WM_CM_Sn[self->screen_number],
                    XCB_XFIXES_SELECTION_EVENT_MASK_SET_SELECTION_OWNER |
                    XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_WINDOW_DESTROY |
                    XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_CLIENT_CLOSE);
            }
        }
    }

    extension_query = xcb_get_extension_data(self->xcb_connection, &xcb_shape_id);
    if ( ! extension_query->present )
        g_warning("No Shape extension");
    else
        self->shape = TRUE;

    xcb_flush(self->xcb_connection);
    _eventd_nd_xcb_check_geometry(self);

    return TRUE;
}

static void
_eventd_nd_xcb_stop(EventdNdBackendContext *self)
{
    if ( self->custom_map )
        xcb_free_colormap(self->xcb_connection, self->map);

    xcb_ewmh_connection_wipe(&self->ewmh);

    g_hash_table_unref(self->bubbles);
    g_water_xcb_source_free(self->source);
    self->bubbles = NULL;
    self->source = NULL;
}

static void
_eventd_nd_xcb_surface_draw(EventdNdSurface *self)
{
    xcb_clear_area(self->context->xcb_connection, TRUE, self->window, 0, 0, 0, 0);
    self->context->nd->notification_draw(self->notification, self->surface, self->context->compositing);
    xcb_flush(self->context->xcb_connection);
}

static void
_eventd_nd_xcb_surface_button_release_event(EventdNdSurface *self)
{
    self->context->nd->notification_dismiss(self->notification);
}

static void
_eventd_nd_xcb_surface_shape(EventdNdSurface *self)
{
    EventdNdBackendContext *context = self->context;

    if ( context->compositing || ( ! context->shape ) )
        return;

    xcb_pixmap_t shape_id;
    cairo_t *cr;
    cairo_surface_t *shape;

    shape_id = xcb_generate_id(context->xcb_connection);
    xcb_create_pixmap(context->xcb_connection, 1,
                      shape_id, context->screen->root,
                      self->width, self->height);

    shape = cairo_xcb_surface_create_for_bitmap(context->xcb_connection, context->screen, shape_id, self->width, self->height);
    cr = cairo_create(shape);

    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_source_rgba(cr, 1, 1, 1, 1);
    self->context->nd->notification_shape(self->notification, cr);
    cairo_fill(cr);

    cairo_destroy(cr);
    cairo_surface_flush(shape);
    cairo_surface_destroy(shape);

    xcb_shape_mask(context->xcb_connection,
                   XCB_SHAPE_SO_INTERSECT, XCB_SHAPE_SK_BOUNDING,
                   self->window, 0, 0, shape_id);

    xcb_free_pixmap(context->xcb_connection, shape_id);
}

static EventdNdSurface *
_eventd_nd_xcb_surface_new(EventdNdBackendContext *context, EventdNdNotification *notification, gint width, gint height)
{
    guint32 selmask =  XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP;
    guint32 selval[] = { 0, 0, 1, XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE, context->map };
    EventdNdSurface *self;

    self = g_new0(EventdNdSurface, 1);

    self->notification = notification;

    self->context = context;
    self->width = width;
    self->height = height;

    self->window = xcb_generate_id(context->xcb_connection);
    xcb_create_window(context->xcb_connection,
                                       context->depth,                /* depth         */
                                       self->window,
                                       context->screen->root,         /* parent window */
                                       0, 0,                          /* x, y          */
                                       width, height,                 /* width, height */
                                       0,                             /* border_width  */
                                       XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class         */
                                       context->visual->visual_id,    /* visual        */
                                       selmask, selval);              /* masks         */

    self->surface = cairo_xcb_surface_create(self->context->xcb_connection, self->window, self->context->visual, self->width, self->height);

    g_hash_table_insert(context->bubbles, GUINT_TO_POINTER(self->window), self);

    _eventd_nd_xcb_surface_shape(self);

    return self;
}

static void
_eventd_nd_xcb_surface_update(EventdNdSurface *self, gint width, gint height)
{
    self->width = width;
    self->height = height;

    guint16 mask = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    guint32 vals[] = { width, height };

    xcb_configure_window(self->context->xcb_connection, self->window, mask, vals);
    cairo_xcb_surface_set_size(self->surface, width, height);

    _eventd_nd_xcb_surface_shape(self);
}

static void
_eventd_nd_xcb_surface_free(EventdNdSurface *self)
{
    if ( self == NULL )
        return;

    EventdNdBackendContext *context = self->context;

    g_hash_table_remove(context->bubbles, GUINT_TO_POINTER(self->window));

    if ( self->mapped )
        xcb_unmap_window(context->xcb_connection, self->window);
    cairo_surface_flush(self->surface);
    cairo_surface_destroy(self->surface);
    xcb_destroy_window(context->xcb_connection, self->window);

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

    if ( ! self->mapped )
    {
        xcb_map_window(self->context->xcb_connection, self->window);
        self->mapped = TRUE;
    }
    _eventd_nd_xcb_surface_draw(self);
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
