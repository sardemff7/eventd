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

#include <glib.h>

#include <Windows.h>
#include <libgwater-win.h>

#include <cairo.h>
#include <cairo-win32.h>

#include "libeventd-helpers-config.h"

#include "backend.h"

#define CLASS_NAME "eventd-nd-win-bubble"

typedef struct _EventdNdBackendContext {
    EventdNdInterface *nd;
    WNDCLASSEX window_class;
    ATOM window_class_atom;
    GWaterWinSource *source;
    HWND window;
    RECT geometry;
} EventdNdDisplay;

struct _EventdNdSurface {
    EventdNdDisplay *display;
    EventdNdNotification *notification;
    HWND window;
    gboolean mapped;
    GList *link;
};

static void
_eventd_nd_win_update_geometry(EventdNdBackendContext *self)
{
    SystemParametersInfo(SPI_GETWORKAREA, 0, &self->geometry, 0);

    gint scale = 1;

    self->nd->geometry_update(self->nd->context, self->geometry.right - self->geometry.left, self->geometry.bottom - self->geometry.top, scale);
}

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
    switch ( message )
    {
    case WM_SETTINGCHANGE:
    {
        if ( GetParent(hwnd) != NULL )
            return DefWindowProc(hwnd,message,wParam,lParam);

        EventdNdBackendContext *self;
        self = GetProp(hwnd, "EventdNdBackendContext");
        g_return_val_if_fail(self != NULL, 1);

        switch ( (UINT) wParam )
        {
        case SPI_SETWORKAREA:
            _eventd_nd_win_update_geometry(self);
        break;
        default:
            return DefWindowProc(hwnd,message,wParam,lParam);
        }
    }
    break;
    case WM_LBUTTONUP:
    {
        EventdNdSurface *self;
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
    self->window_class.hbrBackground = (HBRUSH)0;
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

    self->window = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, self->window_class.lpszClassName, "Control", WS_POPUP, 0, 0, 0, 0, NULL, NULL, NULL, NULL);

    SetProp(self->window, "EventdNdBackendContext", self);

    _eventd_nd_win_update_geometry(self);

    return self;
}

static void
_eventd_nd_win_uninit(EventdNdBackendContext *self_)
{
    EventdNdDisplay *self = (EventdNdDisplay *) self_;

    RemoveProp(self->window, "EventdNdBackendContext");
    DestroyWindow(self->window);

    g_water_win_source_free(self->source);

    g_free(self);
}

static void
_eventd_nd_win_surface_update(EventdNdSurface *self, gint width, gint height)
{
    cairo_surface_t *surface;
    HDC dc;
    surface = cairo_win32_surface_create_with_dib(CAIRO_FORMAT_ARGB32, width, height);
    dc = cairo_win32_surface_get_dc(surface);

    self->display->nd->notification_draw(self->notification, surface, TRUE);

    POINT ptZero = { 0 };
    SIZE size = { width, height };
    BLENDFUNCTION blend = {
        .BlendOp = AC_SRC_OVER,
        .SourceConstantAlpha = 255,
        .AlphaFormat = AC_SRC_ALPHA,
    };
    UpdateLayeredWindow(self->window, NULL, NULL, &size, dc, &ptZero, 0, &blend, ULW_ALPHA);

    cairo_surface_destroy(surface);
}

static EventdNdSurface *
_eventd_nd_win_surface_new(EventdNdDisplay *display, EventdNdNotification *notification, gint width, gint height)
{
    EventdNdSurface *self;
    self = g_new0(EventdNdSurface, 1);
    self->display = display;

    self->notification = notification;

    self->window = CreateWindowEx(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, display->window_class.lpszClassName, "Bubble", WS_POPUP, 0, 0, width, height, display->window, NULL, NULL, NULL);

    SetProp(self->window, "EventdNdSurface", self);

    _eventd_nd_win_surface_update(self, width, height);

    return self;
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
