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

#include "backend.h"

#define CLASS_NAME "eventd-nd-win-bubble"

typedef struct _EventdNdBackendContext {
    EventdNdInterface *nd;
    WNDCLASSEX window_class;
    ATOM window_class_atom;
    GWaterWinSource *source;
    RECT geometry;
} EventdNdDisplay;

struct _EventdNdSurface {
    EventdNdDisplay *display;
    EventdNdNotification *notification;
    HWND window;
    gboolean mapped;
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

        cairo_destroy(cr);

        self->display->nd->notification_draw(self->notification, surface);

        cairo_surface_destroy(surface);

        EndPaint(hwnd, &ps);
    }
    break;
    case WM_LBUTTONUP:
    {
        self = GetProp(hwnd, "EventdNdSurface");
        g_return_val_if_fail(self != NULL, 1);

        self->display->nd->notification_dismiss(self->notification);
    }
    break;
    default:
        return DefWindowProc(hwnd,message,wParam,lParam);
    }
    return 0;
}

static EventdNdBackendContext *
_eventd_nd_win_init(EventdNdInterface *nd)
{
    EventdNdDisplay *self;
    self = g_new0(EventdNdDisplay, 1);
    self->nd = nd;

    self->window_class.cbSize        = sizeof(WNDCLASSEX);
    self->window_class.style         = CS_HREDRAW | CS_VREDRAW;
    self->window_class.lpfnWndProc   = _eventd_nd_win_surface_event_callback;
    self->window_class.hCursor       = LoadCursor(NULL, IDC_ARROW);
    self->window_class.hbrBackground =(HBRUSH)COLOR_WINDOW;
    self->window_class.lpszClassName = CLASS_NAME;

    self->window_class_atom = RegisterClassEx(&self->window_class);
    if ( self->window_class_atom == 0 )
    {
        DWORD error;
        error = GetLastError();
        g_warning("Couldn't register class: %s", g_win32_error_message(error));
        g_free(self);
        return NULL;
    }

    self->source = g_water_win_source_new(NULL, QS_PAINT|QS_MOUSEBUTTON);

    SystemParametersInfo(SPI_GETWORKAREA, 0, &self->geometry, 0);
    self->nd->geometry_update(self->nd->context, self->geometry.right, self->geometry.bottom);

    return self;
}

static void
_eventd_nd_win_uninit(EventdNdBackendContext *self_)
{
    EventdNdDisplay *self = (EventdNdDisplay *) self_;

    g_water_win_source_unref(self->source);

    g_free(self);
}

static EventdNdSurface *
_eventd_nd_win_surface_new(EventdNdDisplay *display, EventdNdNotification *notification, gint width, gint height)
{
    EventdNdSurface *self;
    self = g_new0(EventdNdSurface, 1);
    self->display = display;

    self->notification = notification;

    self->window = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, display->window_class.lpszClassName, "Bubble", WS_POPUP, 0, 0, width, height, NULL, NULL, NULL, NULL);
    SetLayeredWindowAttributes(self->window, RGB(255, 0, 0), 255, LWA_COLORKEY);

    SetProp(self->window, "EventdNdSurface", self);

    return self;
}

static void
_eventd_nd_win_surface_update(EventdNdSurface *self, gint width, gint height)
{
    SetWindowPos(self->window, NULL, 0, 0, width, height, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
}

static void
_eventd_nd_win_surface_free(EventdNdSurface *self)
{
    RemoveProp(self->window, "EventdNdSurface");

    DestroyWindow(self->window);

    g_free(self);
}

static gpointer
_eventd_nd_win_move_begin(EventdNdBackendContext *context, gsize count)
{
    return BeginDeferWindowPos(count);
}

static void
_eventd_nd_win_move_surface(EventdNdSurface *self, gint x, gint y, gpointer wp)
{
    x += self->display->geometry.left;
    y += self->display->geometry.top;
    DeferWindowPos(wp, self->window, NULL, x, y, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);

    if ( ! self->mapped )
    {
        ShowWindow(self->window, SW_SHOWNA);
        self->mapped = TRUE;
    }
}

static void
_eventd_nd_win_move_end(EventdNdBackendContext *context, gpointer wp)
{
    EndDeferWindowPos(wp);
}

EVENTD_EXPORT
void
eventd_nd_backend_get_info(EventdNdBackend *backend)
{
    backend->init = _eventd_nd_win_init;
    backend->uninit = _eventd_nd_win_uninit;

    backend->surface_new    = _eventd_nd_win_surface_new;
    backend->surface_update = _eventd_nd_win_surface_update;
    backend->surface_free   = _eventd_nd_win_surface_free;

    backend->move_begin   = _eventd_nd_win_move_begin;
    backend->move_surface = _eventd_nd_win_move_surface;
    backend->move_end     = _eventd_nd_win_move_end;
}
