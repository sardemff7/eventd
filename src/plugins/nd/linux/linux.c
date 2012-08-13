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

#include <eventd-nd-backend.h>

#define FRAMEBUFFER_TARGET_PREFIX "/dev/tty"

struct _EventdNdBackendContext {
    EventdNdContext *nd;
    EventdNdInterface *nd_interface;
};

struct _EventdNdDisplay {
    gint fd;
    guchar *buffer;
    guint64 screensize;
    gint stride;
    gint channels;
    gint width;
    gint height;
};

struct _EventdNdSurface {
    EventdNdDisplay *display;
    cairo_surface_t *bubble;
    guchar *buffer;
    gint stride;
    gint channels;
};

static EventdNdBackendContext *
_eventd_nd_linux_init(EventdNdContext *nd, EventdNdInterface *nd_interface)
{
    EventdNdBackendContext *context;

    context = g_new0(EventdNdBackendContext, 1);

    context->nd = nd;
    context->nd_interface = nd_interface;

    return context;
}

static void
_eventd_nd_linux_uninit(EventdNdBackendContext *context)
{
    g_free(context);
}

static gboolean
_eventd_nd_linux_display_test(EventdNdBackendContext *context, const gchar *target)
{
    return g_str_has_prefix(target, FRAMEBUFFER_TARGET_PREFIX);
}

static EventdNdDisplay *
_eventd_nd_linux_display_new(EventdNdBackendContext *context, const gchar *target)
{
    EventdNdDisplay *display;
    struct fb_fix_screeninfo finfo;
    struct fb_var_screeninfo vinfo;

    display = g_new0(EventdNdDisplay, 1);

    display->fd = g_open("/dev/fb0", O_RDWR);
    if ( display->fd == -1 )
    {
        g_warning("Couldn't open framebuffer device: %s", g_strerror(errno));
        goto fail;
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
        g_warning("Couldn't map framebuffer device to memory: %s", strerror(errno));
        goto fail;
    }


    return display;

fail:
    g_free(display);
    return NULL;
}

static void
_eventd_nd_linux_display_free(EventdNdDisplay *display)
{
    munmap(display->buffer, display->screensize);
    close(display->fd);

    g_free(display);
}

static inline guchar
alpha_div(guchar c, guchar a)
{
    guint16 t;
    return c;
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
}
static EventdNdSurface *
_eventd_nd_linux_surface_new(EventdEvent *event, EventdNdDisplay *display, cairo_surface_t *bubble, cairo_surface_t *shape)
{
    EventdNdSurface *self;

    self = g_new0(EventdNdSurface, 1);
    self->display = display;

    self->bubble = cairo_surface_reference(bubble);
    self->stride = display->stride;
    self->channels = display->channels;

    gint x, y;

    x = 0;
    y = 0;

    self->buffer = display->buffer + x * display->channels + y * display->stride;

    gint width;
    gint height;

    guchar *pixels, *line;
    const guchar *cpixels, *cpixels_end, *cline, *cline_end;
    gint cstride, clo;

    width = cairo_image_surface_get_width(self->bubble);
    height = cairo_image_surface_get_height(self->bubble);

    cpixels = cairo_image_surface_get_data(self->bubble);
    cstride = cairo_image_surface_get_stride(self->bubble);
    cpixels_end = cpixels + cstride * height;
    clo =  width << 2;

    pixels = self->buffer;

    while ( cpixels < cpixels_end )
    {
        line = pixels;
        cline = cpixels;
        cline_end = cline + clo;

        while ( cline < cline_end )
        {
            line[0] = alpha_div(cline[0], cline[3]);
            line[1] = alpha_div(cline[1], cline[3]);
            line[2] = alpha_div(cline[2], cline[3]);
            line[3] = ~cline[3];

            line += self->channels;
            cline += 4;
        }

        pixels += self->stride;
        cpixels += cstride;
    }

    return self;
}

static void
_eventd_nd_linux_surface_free(EventdNdSurface *self)
{
    cairo_surface_destroy(self->bubble);

    g_free(self);
}

EVENTD_EXPORT const gchar *eventd_nd_backend_id = "eventd-nd-linux";
EVENTD_EXPORT
void
eventd_nd_backend_get_info(EventdNdBackend *backend)
{
    backend->init = _eventd_nd_linux_init;
    backend->uninit = _eventd_nd_linux_uninit;

    backend->display_test = _eventd_nd_linux_display_test;
    backend->display_new = _eventd_nd_linux_display_new;
    backend->display_free = _eventd_nd_linux_display_free;

    backend->surface_new  = _eventd_nd_linux_surface_new;
    backend->surface_free = _eventd_nd_linux_surface_free;
}
