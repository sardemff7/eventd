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

#include <glib.h>

#include <Windows.h>
#include <libgwater-win.h>

#include <cairo.h>
#include <cairo-win32.h>

#include <libeventd-helpers-config.h>

#include "backend-win.h"

#define CLASS_NAME "eventd-nd-win-bubble"

typedef enum {
    EVENTD_ND_WIN_ANCHOR_TOP,
    EVENTD_ND_WIN_ANCHOR_TOP_LEFT,
    EVENTD_ND_WIN_ANCHOR_TOP_RIGHT,
    EVENTD_ND_WIN_ANCHOR_BOTTOM,
    EVENTD_ND_WIN_ANCHOR_BOTTOM_LEFT,
    EVENTD_ND_WIN_ANCHOR_BOTTOM_RIGHT,
} EventdNdWinCornerAnchor;

static const gchar * const _eventd_nd_win_corner_anchors[] = {
    [EVENTD_ND_WIN_ANCHOR_TOP]          = "top",
    [EVENTD_ND_WIN_ANCHOR_TOP_LEFT]     = "top left",
    [EVENTD_ND_WIN_ANCHOR_TOP_RIGHT]    = "top right",
    [EVENTD_ND_WIN_ANCHOR_BOTTOM]       = "bottom",
    [EVENTD_ND_WIN_ANCHOR_BOTTOM_LEFT]  = "bottom left",
    [EVENTD_ND_WIN_ANCHOR_BOTTOM_RIGHT] = "bottom right",
};

typedef struct _EventdNdBackendContext {
    EventdNdContext *nd;
    struct {
        EventdNdWinCornerAnchor anchor;
        gboolean reverse;
        gint margin;
        gint spacing;
    } placement;
    WNDCLASSEX window_class;
    ATOM window_class_atom;
    RECT base_geometry;
    GWaterWinSource *source;
    GQueue *windows;
} EventdNdDisplay;

struct _EventdNdSurface {
    EventdNdDisplay *display;
    EventdEvent *event;
    HWND window;
    cairo_surface_t *bubble;
    GList *link;
};

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define RED_BYTE 2
#define GREEN_BYTE 1
#define BLUE_BYTE 0
#define ALPHA_BYTE 3
#else
#define RED_BYTE 1
#define GREEN_BYTE 2
#define BLUE_BYTE 3
#define ALPHA_BYTE 0
#endif
#define RGBA(r, g, b, a) (r + (g << 8) + (b << 16) + (a << 24))
LRESULT CALLBACK
_eventd_nd_win_surface_event_callback(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    EventdNdSurface *self;
    switch ( message )
    {
    case WM_PAINT:
    {
        self = GetProp(hwnd, "EventdNdSurface");
        g_return_val_if_fail(self != NULL, 1);

        HDC dc;
        PAINTSTRUCT ps;
        dc = BeginPaint(hwnd, &ps);

        cairo_surface_t *surface;
        surface = cairo_win32_surface_create(dc);

        cairo_t *cr;
        cr = cairo_create(surface);

        cairo_set_source_rgb(cr, 255, 0, 0);
        cairo_paint(cr);

        cairo_set_source_surface(cr, self->bubble, 0, 0);
        cairo_paint(cr);

        cairo_destroy(cr);
        cairo_surface_destroy(surface);

        EndPaint(hwnd, &ps);
    }
    break;
    case WM_LBUTTONUP:
    {
        self = GetProp(hwnd, "EventdNdSurface");
        g_return_val_if_fail(self != NULL, 1);
        /*
         * FIXME: add back
        eventd_event_end(self->event, EVENTD_EVENT_END_REASON_USER_DISMISS);
         */
    }
    default:
        return DefWindowProc(hwnd,message,wParam,lParam);
    }
    return 0;
}

EventdNdBackendContext *
eventd_nd_win_init(EventdNdContext *nd)
{
    EventdNdDisplay *self;
    self = g_new0(EventdNdDisplay, 1);
    self->nd = nd;

    /* default bubble position */
    self->placement.anchor    = EVENTD_ND_WIN_ANCHOR_TOP_RIGHT;
    self->placement.margin    = 13;
    self->placement.spacing   = 13;

    SystemParametersInfo(SPI_GETWORKAREA, 0, &self->base_geometry, 0);

    self->window_class.cbSize        = sizeof(WNDCLASSEX);
    self->window_class.style         = CS_HREDRAW | CS_VREDRAW;
    self->window_class.lpfnWndProc   = _eventd_nd_win_surface_event_callback;
    self->window_class.hCursor       = LoadCursor(NULL, IDC_ARROW);
    self->window_class.hbrBackground =(HBRUSH)COLOR_WINDOW;
    self->window_class.lpszClassName = CLASS_NAME;

    self->source = g_water_win_source_new(NULL, QS_PAINT|QS_MOUSEBUTTON);

    self->windows = g_queue_new();

    return self;
}

void
eventd_nd_win_uninit(EventdNdBackendContext *self_)
{
    EventdNdDisplay *self = (EventdNdDisplay *) self_;

    g_water_win_source_unref(self->source);

    g_free(self);
}

void
eventd_nd_win_global_parse(EventdNdBackendContext *self_, GKeyFile *config_file)
{
    EventdNdDisplay *self = (EventdNdDisplay *) self_;

    if ( ! g_key_file_has_group(config_file, "NotificationWin") )
        return;

    Int integer;
    guint64 enum_value;
    gboolean boolean;

    if ( evhelpers_config_key_file_get_enum(config_file, "NotificationWin", "Anchor", _eventd_nd_win_corner_anchors, G_N_ELEMENTS(_eventd_nd_win_corner_anchors), &enum_value) == 0 )
        self->placement.anchor = enum_value;

    if ( evhelpers_config_key_file_get_boolean(config_file, "NotificationWin", "OldestFirst", &boolean) == 0 )
        self->placement.reverse = boolean;

    if ( evhelpers_config_key_file_get_int(config_file, "NotificationWin", "Margin", &integer) == 0 )
        self->placement.margin = integer.value;

    if ( evhelpers_config_key_file_get_int(config_file, "NotificationWin", "Spacing", &integer) == 0 )
        self->placement.spacing = integer.value;
}

gboolean
eventd_nd_win_start(EventdNdBackendContext *self, const gchar *target)
{
    self->window_class_atom = RegisterClassEx(&self->window_class);
    if ( self->window_class_atom == 0 )
    {
        DWORD error;
        error = GetLastError();
        g_warning("Couldn't register class: %s", g_win32_error_message(error));
        g_free(self);
        return FALSE;
    }

    return TRUE;
}

void
eventd_nd_win_stop(EventdNdDisplay *self)
{
}

static void
_eventd_nd_win_update_surfaces(EventdNdDisplay *self)
{
    GList *surface_;
    EventdNdSurface *surface;
    HDWP wp;
    gint x, y, full_width, full_height;

    gboolean right, center, bottom;
    right = ( self->placement.anchor == EVENTD_ND_WIN_ANCHOR_TOP_RIGHT ) || ( self->placement.anchor == EVENTD_ND_WIN_ANCHOR_BOTTOM_RIGHT );
    center = ( self->placement.anchor == EVENTD_ND_WIN_ANCHOR_TOP ) || ( self->placement.anchor == EVENTD_ND_WIN_ANCHOR_BOTTOM );
    bottom = ( self->placement.anchor == EVENTD_ND_WIN_ANCHOR_BOTTOM_LEFT ) || ( self->placement.anchor == EVENTD_ND_WIN_ANCHOR_BOTTOM ) || ( self->placement.anchor == EVENTD_ND_WIN_ANCHOR_BOTTOM_RIGHT );

    wp = BeginDeferWindowPos(g_queue_get_length(self->windows));
    full_width = 0;
    full_height = 0;

    x = self->placement.margin;
    y = self->placement.margin;
    if ( center )
        x = self->base_geometry.right;
    else if ( right )
        x = self->base_geometry.right- x;
    if ( bottom )
        y = self->base_geometry.bottom - y;
    for ( surface_ = g_queue_peek_head_link(self->windows) ; surface_ != NULL ; surface_ = g_list_next(surface_) )
    {
        gint width;
        gint height;
        surface = surface_->data;

        width = cairo_image_surface_get_width(surface->bubble);
        height = cairo_image_surface_get_height(surface->bubble);

        if ( bottom )
            y -= height;

        DeferWindowPos(wp, surface->window, NULL, center ? ( x / 2 - width / 2 ) : right ? ( x - width ) : x, y, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);

        if ( bottom )
            y -= self->placement.spacing;
        else
            y += height + self->placement.spacing;

        if ( width > full_width )
            full_width = width;
        full_height += height;
    }
    EndDeferWindowPos(wp);
}

EventdNdSurface *
eventd_nd_win_surface_new(EventdNdDisplay *display, EventdEvent *event, cairo_surface_t *bubble)
{
    gint width, height;

    width = cairo_image_surface_get_width(bubble);
    height = cairo_image_surface_get_height(bubble);

    EventdNdSurface *self;
    self = g_new0(EventdNdSurface, 1);
    self->display = display;

    self->event = eventd_event_ref(event);
    self->bubble = cairo_surface_reference(bubble);

    self->window = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, display->window_class.lpszClassName, "Bubble", WS_POPUP, 0, 0, width, height, NULL, NULL, NULL, NULL);
    SetLayeredWindowAttributes(self->window, RGB(255, 0, 0), 255, LWA_COLORKEY);

    if ( display->placement.reverse )
    {
        g_queue_push_tail(display->windows, self);
        self->link = g_queue_peek_tail_link(display->windows);
    }
    else
    {
        g_queue_push_head(display->windows, self);
        self->link = g_queue_peek_head_link(display->windows);
    }

    SetProp(self->window, "EventdNdSurface", self);

    _eventd_nd_win_update_surfaces(display);
    ShowWindow(self->window, SW_SHOWNA);

    return self;
}

void
eventd_nd_win_surface_free(EventdNdSurface *self)
{
    EventdNdDisplay *display = self->display;

    RemoveProp(self->window, "EventdNdSurface");

    g_queue_delete_link(display->windows, self->link);

    DestroyWindow(self->window);

    cairo_surface_destroy(self->bubble);
    eventd_event_unref(self->event);

    g_free(self);

    _eventd_nd_win_update_surfaces(display);
}

void
eventd_nd_win_surface_update(EventdNdSurface *self, cairo_surface_t *bubble)
{
    cairo_surface_destroy(self->bubble);
    self->bubble = cairo_surface_reference(bubble);

    gint width, height;

    width = cairo_image_surface_get_width(bubble);
    height = cairo_image_surface_get_height(bubble);

    SetWindowPos(self->window, NULL, 0, 0, width, height, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);

    _eventd_nd_win_update_surfaces(self->display);
}
