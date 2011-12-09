/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011 Quentin "Sardem FF7" Glidic
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
#include <pango/pango.h>

#include "../types.h"
#include "../style-internal.h"

#include "fb.h"

struct _EventdNdDisplay {
    gint fd;
    guchar *buffer;
    guint64 screensize;
    gint stride;
    gint channels;
    gint x;
    gint y;
};

struct _EventdNdSurface {
    cairo_surface_t *bubble;
    guchar *buffer;
    guchar *save;
    gint stride;
    gint channels;
};

EventdNdDisplay *
eventd_nd_fb_display_new(const gchar *target, EventdNdStyle *style)
{
    EventdNdDisplay *context;
    struct fb_fix_screeninfo finfo;
    struct fb_var_screeninfo vinfo;

    context = g_new0(EventdNdDisplay, 1);

    context->fd = g_open("/dev/fb0", O_RDWR);
    if ( context->fd == -1 )
    {
        g_warning("Couldn’t open framebuffer device: %s", strerror(errno));
        goto fail;
    }

    if ( ioctl(context->fd, FBIOGET_FSCREENINFO, &finfo) == -1 )
    {
        g_warning("Couldn’t get framebuffer fixed info: %s", strerror(errno));
        goto fail;
    }

    if ( ioctl(context->fd, FBIOGET_VSCREENINFO, &vinfo) == -1 )
    {
        g_warning("Couldn’t get framebuffer variable info: %s", strerror(errno));
        goto fail;
    }

    context->channels = vinfo.bits_per_pixel >> 3;
    context->stride = finfo.line_length;

    switch ( style->bubble_anchor )
    {
    case EVENTD_ND_STYLE_ANCHOR_TOP_LEFT:
        context->x = style->bubble_margin;
        context->y = style->bubble_margin;
    break;
    case EVENTD_ND_STYLE_ANCHOR_TOP_RIGHT:
        context->x = - vinfo.xres + style->bubble_margin;
        context->y = style->bubble_margin;
    break;
    case EVENTD_ND_STYLE_ANCHOR_BOTTOM_LEFT:
        context->x = style->bubble_margin;
        context->y = - vinfo.yres + style->bubble_margin;
    break;
    case EVENTD_ND_STYLE_ANCHOR_BOTTOM_RIGHT:
        context->x = - vinfo.xres + style->bubble_margin;
        context->y = - vinfo.yres + style->bubble_margin;
    break;
    }

    context->screensize = ( vinfo.xoffset * ( vinfo.yres - 1 ) + vinfo.xres * vinfo.yres ) * context->channels;

    context->buffer = mmap(NULL, context->screensize, PROT_READ|PROT_WRITE, MAP_SHARED, context->fd, ( vinfo.xoffset ) * context->channels + ( vinfo.yoffset ) * context->stride);
    if ( context->buffer == (void *)-1 )
    {
        g_warning("Couldn’t map framebuffer device to memory: %s", strerror(errno));
        goto fail;
    }


    return context;

fail:
    g_free(context);
    return NULL;
}

void
eventd_nd_fb_display_free(gpointer data)
{
    EventdNdDisplay *display = data;

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

EventdNdSurface *
eventd_nd_fb_surface_new(EventdNdDisplay *display, gint width, gint height, cairo_surface_t *bubble, cairo_surface_t *shape)
{
    EventdNdSurface *self;
    gint x, y;

    x = display->x;
    y = display->y;

    if ( x < 0 )
        x = - x - width;
    if ( y < 0 )
        y = - y - height;

    self = g_new0(EventdNdSurface, 1);

    self->bubble = cairo_surface_reference(bubble);
    self->buffer = display->buffer + x * display->channels + y * display->stride;
    self->stride = display->stride;
    self->channels = display->channels;

    self->save = g_malloc(self->channels * cairo_image_surface_get_height(self->bubble) * cairo_image_surface_get_width(self->bubble));

    return self;
}

void
eventd_nd_fb_surface_show(EventdNdSurface *self)
{
    guchar *spixels, *sline;
    gint sstride;
    guchar *pixels, *line;
    const guchar *cpixels, *cpixels_end, *cline, *cline_end;
    gint cstride, clo;
    gint w;

    w = cairo_image_surface_get_width(self->bubble);

    cpixels = cairo_image_surface_get_data(self->bubble);
    cstride = cairo_image_surface_get_stride(self->bubble);
    cpixels_end = cpixels + cstride * cairo_image_surface_get_height(self->bubble);
    clo =  w << 2;

    spixels = self->save;
    sstride = w * self->channels;

    pixels = self->buffer;

    while ( cpixels < cpixels_end )
    {
        sline = spixels;
        line = pixels;
        cline = cpixels;
        cline_end = cline + clo;

        while ( cline < cline_end )
        {
            sline[0] = line[0];
            sline[1] = line[1];
            sline[2] = line[2];
            sline[3] = line[3];

            line[0] = alpha_div(cline[0], cline[3]);
            line[1] = alpha_div(cline[1], cline[3]);
            line[2] = alpha_div(cline[2], cline[3]);
            line[3] = ~cline[3];

            sline += self->channels;
            line += self->channels;
            cline += 4;
        }

        spixels += sstride;
        pixels += self->stride;
        cpixels += cstride;
    }
}

void
eventd_nd_fb_surface_hide(EventdNdSurface *self)
{
    guchar *pixels, *line;
    const guchar *spixels, *spixels_end, *sline, *sline_end;
    gint sstride, slo;

    spixels = self->save;
    sstride = slo = self->channels * cairo_image_surface_get_width(self->bubble);
    spixels_end = self->save + sstride * cairo_image_surface_get_height(self->bubble);

    pixels = self->buffer;

    while ( spixels < spixels_end )
    {
        sline = spixels;
        sline_end = sline + slo;
        line = pixels;

        while ( sline < sline_end )
        {
            line[0] = sline[0];
            line[1] = sline[1],
            line[2] = sline[2];
            line[3] = sline[3];

            sline += self->channels;
            line += self->channels;
        }

        pixels += self->stride;
        spixels += sstride;
    }
}

void
eventd_nd_fb_surface_free(gpointer data)
{
    EventdNdSurface *self = data;

    cairo_surface_destroy(self->bubble);

    g_free(self->save);

    g_free(self);
}
