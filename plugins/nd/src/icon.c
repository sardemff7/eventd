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
#include <glib-object.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>

#include <libeventd-helpers-config.h>

#include "style.h"
#include "pixbuf.h"

#include "icon.h"

static cairo_surface_t *
_eventd_nd_cairo_limit_size(cairo_surface_t *source, gint max_width, gint max_height)
{
    gdouble s = 1.0;
    gint width, height;
    cairo_surface_t *surface;
    cairo_t *cr;
    cairo_pattern_t *pattern;
    cairo_matrix_t matrix;

    width = cairo_image_surface_get_width(source);
    height = cairo_image_surface_get_height(source);

    if ( ( width > max_width ) && ( height > max_height ) )
    {
        if ( width > height )
            s = (gdouble) max_width / (gdouble) width;
        else
            s = (gdouble) max_height / (gdouble) height;
    }
    else if ( width > max_width )
        s = (gdouble) max_width / (gdouble) width;
    else if ( height > max_height )
        s = (gdouble) max_height / (gdouble) height;
    else
        return source;

    width *= s;
    height *= s;

    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cr = cairo_create(surface);

    cairo_matrix_init_scale(&matrix, 1. / s, 1. / s);

    pattern = cairo_pattern_create_for_surface(source);
    cairo_pattern_set_matrix(pattern, &matrix);

    cairo_set_source(cr, pattern);
    cairo_paint(cr);

    cairo_pattern_destroy(pattern);
    cairo_destroy(cr);
    cairo_surface_destroy(source);

    return surface;
}

/*
 * _eventd_nd_cairo_get_icon_surface and alpha_mult
 * are inspired by gdk_cairo_set_source_pixbuf
 * GDK is:
 *     Copyright (C) 2011-2015 Red Hat, Inc.
 */
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

static inline guchar
alpha_mult(guchar c, guchar a)
{
    guint16 t;
    switch ( a )
    {
    case 0xff:
        return c;
    case 0x00:
        return 0x00;
    default:
        t = c * a + 0x7f;
        return ((t >> 8) + t) >> 8;
    }
}

static cairo_surface_t *
_eventd_nd_cairo_get_surface_from_pixbuf(GdkPixbuf *pixbuf)
{
    gint width, height;
    const guchar *pixels;
    gint stride;
    gboolean alpha;

    if ( pixbuf == NULL )
        return NULL;

    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    pixels = gdk_pixbuf_get_pixels(pixbuf);
    stride = gdk_pixbuf_get_rowstride(pixbuf);
    alpha = gdk_pixbuf_get_has_alpha(pixbuf);

    cairo_surface_t *surface = NULL;

    gint cstride;
    guint lo, o;
    guchar a = 0xff;
    const guchar *pixels_end, *line, *line_end;
    guchar *cpixels, *cline;

    pixels_end = pixels + height * stride;
    o = alpha ? 4 : 3;
    lo = o * width;

    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cpixels = cairo_image_surface_get_data(surface);
    cstride = cairo_image_surface_get_stride(surface);

    cairo_surface_flush(surface);
    while ( pixels < pixels_end )
    {
        line = pixels;
        line_end = line + lo;
        cline = cpixels;

        while ( line < line_end )
        {
            if ( alpha )
                a = line[3];
            cline[RED_BYTE] = alpha_mult(line[0], a);
            cline[GREEN_BYTE] = alpha_mult(line[1], a);
            cline[BLUE_BYTE] = alpha_mult(line[2], a);
            cline[ALPHA_BYTE] = a;

            line += o;
            cline += 4;
        }

        pixels += stride;
        cpixels += cstride;
    }
    cairo_surface_mark_dirty(surface);

    return surface;
}

static cairo_surface_t *
_eventd_nd_cairo_image_process(GdkPixbuf *pixbuf, EventdNdStyle *style, gint *width, gint *height)
{
    cairo_surface_t *image;

    image = _eventd_nd_cairo_limit_size(_eventd_nd_cairo_get_surface_from_pixbuf(pixbuf),
                                        eventd_nd_style_get_image_max_width(style),
                                        eventd_nd_style_get_image_max_height(style));

    *width = cairo_image_surface_get_width(image) + eventd_nd_style_get_image_margin(style);
    *height = cairo_image_surface_get_height(image);

    return image;
}

static cairo_surface_t *
_eventd_nd_cairo_icon_process_overlay(GdkPixbuf *pixbuf, EventdNdStyle *style, gint *width, gint *height)
{
    cairo_surface_t *icon;
    gint w, h;

    icon = _eventd_nd_cairo_limit_size(_eventd_nd_cairo_get_surface_from_pixbuf(pixbuf),
                                        eventd_nd_style_get_icon_max_width(style),
                                        eventd_nd_style_get_icon_max_height(style));

    w = cairo_image_surface_get_width(icon);
    h = cairo_image_surface_get_height(icon);

    *width += ( w / 4 );
    *height += ( h / 4 );

    return icon;
}

static cairo_surface_t *
_eventd_nd_cairo_icon_process_foreground(GdkPixbuf *pixbuf, EventdNdStyle *style, gint *width, gint *height)
{
    cairo_surface_t *icon;

    icon = _eventd_nd_cairo_limit_size(_eventd_nd_cairo_get_surface_from_pixbuf(pixbuf),
                                        eventd_nd_style_get_icon_max_width(style),
                                        eventd_nd_style_get_icon_max_height(style));

    gint h;

    *width += cairo_image_surface_get_width(icon) + eventd_nd_style_get_icon_margin(style);
    h = cairo_image_surface_get_height(icon);
    *height = MAX(*height, h);

    return icon;
}

static cairo_surface_t *
_eventd_nd_cairo_icon_process_background(GdkPixbuf *pixbuf, EventdNdStyle *style, gint max_width, gint *width, gint *height)
{
    gint fade_width;

    fade_width = gdk_pixbuf_get_width(pixbuf) * eventd_nd_style_get_icon_fade_width(style) / 4;

    if ( ( max_width > -1 ) && ( fade_width > max_width ) )
        return NULL;

    cairo_surface_t *icon;

    icon = _eventd_nd_cairo_icon_process_foreground(pixbuf, style, width, height);

    *width -= fade_width;

    return icon;
}

void
eventd_nd_cairo_image_and_icon_process(EventdNdStyle *style, EventdEvent *event, gint max_width, cairo_surface_t **image, cairo_surface_t **icon, gint *text_x, gint *width, gint *height)
{
    GdkPixbuf *image_pixbuf = NULL;
    GdkPixbuf *icon_pixbuf = NULL;
    const Filename *image_filename = eventd_nd_style_get_template_image(style);
    const Filename *icon_filename = eventd_nd_style_get_template_icon(style);
    gchar *uri;

    uri = evhelpers_filename_get_uri(image_filename, event, "images");
    if ( uri != NULL )
       image_pixbuf = eventd_nd_pixbuf_from_uri(uri, *width, *height);

    uri = evhelpers_filename_get_uri(icon_filename, event, "icons");
    if ( uri != NULL )
        icon_pixbuf = eventd_nd_pixbuf_from_uri(uri, *width, *height);

    *text_x = 0;
    *width = 0;
    *height = 0;
    switch ( eventd_nd_style_get_icon_placement(style) )
    {
    case EVENTD_ND_STYLE_ICON_PLACEMENT_BACKGROUND:
        if ( ( image_pixbuf != NULL )
             && ( ( max_width < 0 ) || ( gdk_pixbuf_get_width(image_pixbuf) < max_width ) ) )
        {
            *image = _eventd_nd_cairo_image_process(image_pixbuf, style, width, height);
            *text_x = *width;
            max_width -= *width;
        }
        if ( ( icon_pixbuf != NULL )
             && ( ( max_width < 0 ) || ( gdk_pixbuf_get_width(icon_pixbuf) < max_width ) ) )
            *icon = _eventd_nd_cairo_icon_process_background(icon_pixbuf, style, max_width, width, height);
    break;
    case EVENTD_ND_STYLE_ICON_PLACEMENT_OVERLAY:
        if ( ( image_pixbuf == NULL ) && ( icon_pixbuf != NULL ) )
        {
            image_pixbuf = icon_pixbuf;
            icon_pixbuf = NULL;
        }
        if ( image_pixbuf != NULL )
        {
            if ( ( max_width < 0 ) || ( gdk_pixbuf_get_width(image_pixbuf) < max_width ) )
            {
                *image = _eventd_nd_cairo_image_process(image_pixbuf, style, width, height);
                max_width -= *width;
            }
            if ( ( icon_pixbuf != NULL )
                 && ( ( max_width < 0 ) || ( gdk_pixbuf_get_width(icon_pixbuf) < max_width ) ) )
                *icon = _eventd_nd_cairo_icon_process_overlay(icon_pixbuf, style, width, height);
            *text_x = *width;
        }
    break;
    case EVENTD_ND_STYLE_ICON_PLACEMENT_FOREGROUND:
        if ( ( image_pixbuf != NULL )
             && ( ( max_width < 0 ) || ( gdk_pixbuf_get_width(image_pixbuf) < max_width ) ) )
        {
            *image = _eventd_nd_cairo_image_process(image_pixbuf, style, width, height);
            *text_x = *width;
            max_width -= *width;
        }
        if ( ( icon_pixbuf != NULL )
             && ( ( max_width < 0 ) || ( gdk_pixbuf_get_width(icon_pixbuf) < max_width ) ) )
            *icon = _eventd_nd_cairo_icon_process_foreground(icon_pixbuf, style, width, height);
    break;
    }
}

static gint
_eventd_nd_cairo_get_valign(EventdNdAnchorVertical anchor, gint height, gint padding, gint surface_height)
{
    switch ( anchor )
    {
    case EVENTD_ND_VANCHOR_TOP:
        return padding;
    case EVENTD_ND_VANCHOR_CENTER:
        return ( height / 2 ) - (surface_height / 2);
    case EVENTD_ND_VANCHOR_BOTTOM:
        return height - padding - surface_height;
    }
    g_assert_not_reached();
}

static void
_eventd_nd_cairo_surface_draw(cairo_t *cr, cairo_surface_t *surface, gint x, gint y)
{
    cairo_set_source_surface(cr, surface, x, y);
    cairo_rectangle(cr, x, y, cairo_image_surface_get_width(surface), cairo_image_surface_get_height(surface));
    cairo_fill(cr);
    cairo_surface_destroy(surface);
}

static void
_eventd_nd_cairo_image_draw(cairo_t *cr, cairo_surface_t *image, EventdNdStyle *style, gint width, gint height)
{
    gint x, y;
    gint padding;

    padding = eventd_nd_style_get_bubble_padding(style);

    x = padding;
    y = padding;
    _eventd_nd_cairo_surface_draw(cr, image, x, y);
}

static void
_eventd_nd_cairo_image_and_icon_draw_overlay(cairo_t *cr, cairo_surface_t *image, cairo_surface_t *icon, EventdNdStyle *style, gint width, gint height)
{
    if ( image == NULL )
        return;

    gint padding;

    padding = eventd_nd_style_get_bubble_padding(style);

    if ( icon == NULL )
        _eventd_nd_cairo_image_draw(cr, image, style, width, height);
    else
    {
        gint image_x, image_y;
        gint icon_x, icon_y;
        gint w, h;

        w = cairo_image_surface_get_width(icon);
        h =  cairo_image_surface_get_height(icon);

        image_x = padding;
        image_y = padding;
        icon_x = image_x + cairo_image_surface_get_width(image) - ( 3 * w / 4 );
        icon_y = image_y + cairo_image_surface_get_height(image) - ( 3 * h / 4 );

        _eventd_nd_cairo_surface_draw(cr, image, image_x, image_y);
        _eventd_nd_cairo_surface_draw(cr, icon, icon_x, icon_y);
    }
}

static void
_eventd_nd_cairo_image_and_icon_draw_foreground(cairo_t *cr, cairo_surface_t *image, cairo_surface_t *icon, EventdNdStyle *style, gint width, gint height)
{
    gint padding;

    padding = eventd_nd_style_get_bubble_padding(style);

    if ( image != NULL )
        _eventd_nd_cairo_image_draw(cr, image, style, width, height);

    if ( icon != NULL )
    {
        gint x, y;

        x = width - padding - cairo_image_surface_get_width(icon);
        y = _eventd_nd_cairo_get_valign(eventd_nd_style_get_icon_anchor(style), height, padding, cairo_image_surface_get_height(icon));
        _eventd_nd_cairo_surface_draw(cr, icon, x, y);
    }
}

static void
_eventd_nd_cairo_image_and_icon_draw_background(cairo_t *cr, cairo_surface_t *image, cairo_surface_t *icon, EventdNdStyle *style, gint width, gint height)
{
    gint padding;

    padding = eventd_nd_style_get_bubble_padding(style);

    if ( image != NULL )
        _eventd_nd_cairo_image_draw(cr, image, style, width, height);

    if ( icon != NULL )
    {
        gint x1, x2, y;
        cairo_pattern_t *mask;

        x2 = width - padding;
        x1 = x2 - cairo_image_surface_get_width(icon);
        y = _eventd_nd_cairo_get_valign(eventd_nd_style_get_icon_anchor(style), height, padding, cairo_image_surface_get_height(icon));

        mask = cairo_pattern_create_linear(x1, 0, x2, 0);
        cairo_pattern_add_color_stop_rgba(mask, 0, 0, 0, 0, 0);
        cairo_pattern_add_color_stop_rgba(mask, eventd_nd_style_get_icon_fade_width(style), 0, 0, 0, 1);

        cairo_set_source_surface(cr, icon, x1, y);
        cairo_mask(cr, mask);
        cairo_surface_destroy(icon);
    }
}

void
eventd_nd_cairo_image_and_icon_draw(cairo_t *cr, cairo_surface_t *image, cairo_surface_t *icon, EventdNdStyle *style, gint width, gint height)
{
    switch ( eventd_nd_style_get_icon_placement(style) )
    {
    case EVENTD_ND_STYLE_ICON_PLACEMENT_BACKGROUND:
        _eventd_nd_cairo_image_and_icon_draw_background(cr, image, icon, style, width, height);
    break;
    case EVENTD_ND_STYLE_ICON_PLACEMENT_OVERLAY:
        _eventd_nd_cairo_image_and_icon_draw_overlay(cr, image, icon, style, width, height);
    break;
    case EVENTD_ND_STYLE_ICON_PLACEMENT_FOREGROUND:
        _eventd_nd_cairo_image_and_icon_draw_foreground(cr, image, icon, style, width, height);
    break;
    }
}
