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

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif /* HAVE_FCNTL_H */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#include <sys/ioctl.h>
#include <linux/fb.h>
#include <sys/mman.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <cairo.h>

#include "backend-linux.h"

#define FRAMEBUFFER_TARGET_PREFIX "/dev/fb"

struct _EventdNdBackendContext {
    EventdNdContext *nd;
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
    cairo_surface_t *bubble;
    guchar *buffer;
    gint stride;
    gint channels;
};

EventdNdBackendContext *
eventd_nd_linux_init(EventdNdContext *nd)
{
    EventdNdBackendContext *context;

    context = g_new0(EventdNdBackendContext, 1);

    context->nd = nd;

    return context;
}

void
eventd_nd_linux_uninit(EventdNdBackendContext *context)
{
    g_free(context);
}

void
eventd_nd_linux_global_parse(EventdNdBackendContext *context, GKeyFile *config_file)
{
}

gboolean
eventd_nd_linux_start(EventdNdBackendContext *context, const gchar *target)
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
        g_warning("Couldn't map framebuffer device to memory: %s", strerror(errno));
        goto fail;
    }

    return TRUE;

fail:
    close(context->fd);
    context->fd = 0;
    return FALSE;
}

void
eventd_nd_linux_stop(EventdNdBackendContext *display)
{
    EventdNdBackendContext *context = display;
    munmap(display->buffer, display->screensize);
    context->buffer = NULL;

    close(display->fd);
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

EventdNdSurface *
eventd_nd_linux_surface_new(EventdNdBackendContext *display, EventdEvent *event, cairo_surface_t *bubble)
{
    EventdNdSurface *self;

    self = g_new0(EventdNdSurface, 1);
    self->display = display;

    self->bubble = cairo_surface_reference(bubble);
    self->stride = display->stride;
    self->channels = display->channels;

    return self;
}

void
eventd_nd_linux_surface_free(EventdNdSurface *self)
{
    cairo_surface_destroy(self->bubble);

    g_free(self);
}

void
eventd_nd_linux_surface_update(EventdNdSurface *self, cairo_surface_t *bubble)
{
    cairo_surface_destroy(self->bubble);
    self->bubble = cairo_surface_reference(bubble);
}

/*
 * TODO: display bubbles again
 */
static void
_eventd_nd_linux_surface_display(EventdNdSurface *self, gint x, gint y)
{
    EventdNdBackendContext *display = self->display;

    if ( x < 0 )
        x += display->width;
    if ( y < 0 )
        y += display->height;

    self->buffer = display->buffer + x * display->channels + y * display->stride;

    gint width;
    gint height;

    width = cairo_image_surface_get_width(self->bubble);
    height = cairo_image_surface_get_height(self->bubble);

    cairo_surface_t *surface;
    cairo_t *cr;

    surface = cairo_image_surface_create_for_data(self->buffer, CAIRO_FORMAT_ARGB32, width, height, self->stride);
    cr = cairo_create(surface);

    cairo_set_source_surface(cr, self->bubble, 0, 0);
    cairo_paint(cr);

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}
