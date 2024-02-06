/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2024 Morgane "Sardem FF7" Glidic
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

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <linux/fb.h>
#include <sys/mman.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <cairo.h>

#include "backend.h"

#define FRAMEBUFFER_TARGET_PREFIX "/dev/fb"

struct _EventdNdBackendContext {
    EventdNdInterface *nd;
    gint fd;
    guchar *buffer;
    guint64 screensize;
    gint stride;
    gint channels;
    gint width;
    gint height;
};

struct _EventdNdSurface {
    EventdNdBackendContext *display;
    EventdNdNotification *notification;
    gint width;
    gint height;
};

static EventdNdBackendContext *
_eventd_nd_fbdev_init(EventdNdInterface *nd, NkBindings *bindings)
{
    EventdNdBackendContext *context;

    context = g_new0(EventdNdBackendContext, 1);

    context->nd = nd;

    return context;
}

static void
_eventd_nd_fbdev_uninit(EventdNdBackendContext *context)
{
    g_free(context);
}

static gboolean
_eventd_nd_fbdev_start(EventdNdBackendContext *context, const gchar *target)
{
    EventdNdBackendContext *display = context;
    struct fb_fix_screeninfo finfo;
    struct fb_var_screeninfo vinfo;

    if ( ! g_str_has_prefix(target, FRAMEBUFFER_TARGET_PREFIX) )
        return FALSE;

    display->fd = g_open(target, O_RDWR);
    if ( display->fd == -1 )
    {
        g_warning("Couldn't open framebuffer device: %s", g_strerror(errno));
        return FALSE;
    }

    if ( ioctl(display->fd, FBIOGET_FSCREENINFO, &finfo) == -1 )
    {
        g_warning("Couldn't get framebuffer fixed info: %s", g_strerror(errno));
        goto fail;
    }

    if ( ioctl(display->fd, FBIOGET_VSCREENINFO, &vinfo) == -1 )
    {
        g_warning("Couldn't get framebuffer variable info: %s", g_strerror(errno));
        goto fail;
    }

    display->channels = vinfo.bits_per_pixel >> 3;
    display->stride = finfo.line_length;

    display->width = vinfo.xres;
    display->height = vinfo.yres;

    display->screensize = ( vinfo.xoffset * ( vinfo.yres - 1 ) + vinfo.xres * vinfo.yres ) * display->channels;

    display->buffer = mmap(NULL, display->screensize, PROT_READ|PROT_WRITE, MAP_SHARED, display->fd, ( vinfo.xoffset ) * display->channels + ( vinfo.yoffset ) * display->stride);
    if ( display->buffer == (void *)-1 )
    {
        g_warning("Couldn't map framebuffer device to memory: %s", g_strerror(errno));
        goto fail;
    }

    context->nd->shaping_update(context->nd->context, EVENTD_ND_SHAPING_COMPOSITING);
    context->nd->geometry_update(context->nd->context, display->width, display->height, _eventd_nd_compute_scale_from_size(display->width, display->height, vinfo.width, vinfo.height));

    return TRUE;

fail:
    g_close(context->fd, NULL);
    context->fd = 0;
    return FALSE;
}

static void
_eventd_nd_fbdev_stop(EventdNdBackendContext *display)
{
    EventdNdBackendContext *context = display;
    munmap(display->buffer, display->screensize);
    context->buffer = NULL;

    g_close(display->fd, NULL);
    context->fd = 0;
}

static inline guchar
alpha_div(guchar c, guchar a)
{
#if 0
    guint16 t;
    switch ( a )
    {
    case 0xff:
        return c;
    case 0x00:
        return 0x00;
    default:
        t = c / a + 0x7f;
        return ((t << 8) + t) << 8;
    }
#endif
    return c;
}

static EventdNdSurface *
_eventd_nd_fbdev_surface_new(EventdNdBackendContext *display, EventdNdNotification *notification, gint width, gint height)
{
    EventdNdSurface *self;

    self = g_new0(EventdNdSurface, 1);
    self->display = display;

    self->notification = notification;
    self->width = width;
    self->height = height;

    return self;
}

static void
_eventd_nd_fbdev_surface_update(EventdNdSurface *self, gint width, gint height)
{
    self->width = width;
    self->height = height;
}

static void
_eventd_nd_fbdev_surface_free(EventdNdSurface *self)
{
    g_free(self);
}

static void
_eventd_nd_fbdev_move_surface(EventdNdSurface *self, gint x, gint y, gpointer data)
{
    EventdNdBackendContext *display = self->display;

    if ( x < 0 )
        x += display->width;
    if ( y < 0 )
        y += display->height;

    guchar *buffer = display->buffer + x * display->channels + y * display->stride;

    cairo_surface_t *surface;

    surface = cairo_image_surface_create_for_data(buffer, CAIRO_FORMAT_ARGB32, self->width, self->height, display->stride);
    self->display->nd->notification_draw(self->notification, surface);
    cairo_surface_destroy(surface);
}

EVENTD_EXPORT
void
eventd_nd_backend_get_info(EventdNdBackend *backend)
{
    backend->init = _eventd_nd_fbdev_init;
    backend->uninit = _eventd_nd_fbdev_uninit;

    backend->start = _eventd_nd_fbdev_start;
    backend->stop  = _eventd_nd_fbdev_stop;

    backend->surface_new    = _eventd_nd_fbdev_surface_new;
    backend->surface_update = _eventd_nd_fbdev_surface_update;
    backend->surface_free   = _eventd_nd_fbdev_surface_free;

    backend->move_surface = _eventd_nd_fbdev_move_surface;
}
