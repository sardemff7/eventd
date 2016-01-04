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

        self->display->nd->surface_remove(self->display->nd->context, eventd_event_get_uuid(self->event));
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

    self->source = g_water_win_source_new(NULL, QS_PAINT|QS_MOUSEBUTTON);

    return self;
}

static void
_eventd_nd_win_uninit(EventdNdBackendContext *self_)
{
    EventdNdDisplay *self = (EventdNdDisplay *) self_;

    g_water_win_source_unref(self->source);

    g_free(self);
}

static void
_eventd_nd_win_global_parse(EventdNdBackendContext *self_, GKeyFile *config_file)
{
}

static gboolean
_eventd_nd_win_start(EventdNdBackendContext *self, const gchar *target)
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

static void
_eventd_nd_win_stop(EventdNdDisplay *self)
{
}

static EventdNdSurface *
_eventd_nd_win_surface_new(EventdNdDisplay *display, EventdEvent *event, cairo_surface_t *bubble)
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

    SetProp(self->window, "EventdNdSurface", self);

    ShowWindow(self->window, SW_SHOWNA);

    return self;
}

static void
_eventd_nd_win_surface_update(EventdNdSurface *self, cairo_surface_t *bubble)
{
    cairo_surface_destroy(self->bubble);
    self->bubble = cairo_surface_reference(bubble);

    gint width, height;

    width = cairo_image_surface_get_width(bubble);
    height = cairo_image_surface_get_height(bubble);

    SetWindowPos(self->window, NULL, 0, 0, width, height, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
}

static void
_eventd_nd_win_surface_free(EventdNdSurface *self)
{
    EventdNdDisplay *display = self->display;

    RemoveProp(self->window, "EventdNdSurface");

    DestroyWindow(self->window);

    cairo_surface_destroy(self->bubble);
    eventd_event_unref(self->event);

    g_free(self);
}

EVENTD_EXPORT
void
eventd_nd_backend_get_info(EventdNdBackend *backend)
{
    backend->init = _eventd_nd_win_init;
    backend->uninit = _eventd_nd_win_uninit;

    backend->global_parse = _eventd_nd_win_global_parse;

    backend->start = _eventd_nd_win_start;
    backend->stop  = _eventd_nd_win_stop;

    backend->surface_new     = _eventd_nd_win_surface_new;
    backend->surface_update  = _eventd_nd_win_surface_update;
    backend->surface_free    = _eventd_nd_win_surface_free;
}
