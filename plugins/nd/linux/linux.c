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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

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

static EventdNdDisplay *
_eventd_nd_linux_display_new(EventdNdBackendContext *context, const gchar *target)
{
    EventdNdDisplay *display;
    struct fb_fix_screeninfo finfo;
    struct fb_var_screeninfo vinfo;

    if ( target == NULL )
        target = g_getenv("TTY");

    if ( target == NULL )
        return NULL;

    if ( ! g_str_has_prefix(target, FRAMEBUFFER_TARGET_PREFIX) )
        return NULL;

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
_eventd_nd_linux_surface_new(EventdEvent *event, EventdNdDisplay *display, cairo_surface_t *bubble)
{
    EventdNdSurface *self;

    self = g_new0(EventdNdSurface, 1);
    self->display = display;

    self->bubble = cairo_surface_reference(bubble);
    self->stride = display->stride;
    self->channels = display->channels;

    return self;
}

static void
_eventd_nd_linux_surface_free(EventdNdSurface *self)
{
    cairo_surface_destroy(self->bubble);

    g_free(self);
}

static void
_eventd_nd_linux_surface_update(EventdNdSurface *self, cairo_surface_t *bubble)
{
    cairo_surface_destroy(self->bubble);
    self->bubble = cairo_surface_reference(bubble);
}

static void
_eventd_nd_linux_surface_display(EventdNdSurface *self, gint x, gint y)
{
    EventdNdDisplay *display = self->display;

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

EVENTD_EXPORT const gchar *eventd_nd_backend_id = "eventd-nd-linux";
EVENTD_EXPORT
void
eventd_nd_backend_get_info(EventdNdBackend *backend)
{
    backend->init = _eventd_nd_linux_init;
    backend->uninit = _eventd_nd_linux_uninit;

    backend->display_new = _eventd_nd_linux_display_new;
    backend->display_free = _eventd_nd_linux_display_free;

    backend->surface_new     = _eventd_nd_linux_surface_new;
    backend->surface_free    = _eventd_nd_linux_surface_free;
    backend->surface_update  = _eventd_nd_linux_surface_update;
    backend->surface_display = _eventd_nd_linux_surface_display;
}
