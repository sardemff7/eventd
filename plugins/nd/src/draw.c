/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2016 Quentin "Sardem FF7" Glidic
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <glib.h>
#include <glib-object.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>

#include <libeventd-event.h>
#include <libeventd-helpers-config.h>

#include "style.h"
#include "pixbuf.h"
#include "blur.h"

#include "cairo.h"


static gssize
_eventd_nd_draw_strccount(const gchar *str, char c)
{
    gssize count = 1;
    for ( ; *str != 0 ; ++str )
    {
        if ( *str == c )
            ++count;
    }
    return count;
}

static gchar *
_eventd_nd_draw_find_n_c(gchar *s, gsize n, gunichar c)
{
    gsize l;
    gchar *r;
    gsize i;

    r = s;
    l = strlen(s);
    for ( i = 0 ; ( r != NULL ) && ( i < n ) ; ++i )
    {
        /* We know how many \n we have */
        r = g_utf8_strchr(r, l - ( r - s ), c);
        ++r;
    }
    return r - 1;
}

static gchar *
_eventd_nd_draw_get_text(EventdNdStyle *style, EventdEvent *event, guint8 *max_lines)
{
    /*
     * This function depends on the current Pango implementation,
     * which is limiting on a per-paragraph basis.
     * If they switch to a per-layout basis, this whole function will be
     * replaced by a simple pango_layout_set_height(-lines) call.
     */

    gchar *text;
    text = evhelpers_format_string_get_string(eventd_nd_style_get_template_text(style), event, NULL, NULL);
    if ( *text == '\0' )
        return NULL;

    guint8 max;
    max = eventd_nd_style_get_text_max_lines(style);

    if ( max < 1 )
        goto ret;

    gssize count;

    if ( ( count = _eventd_nd_draw_strccount(text, '\n') ) <= max )
    {
        *max_lines = max / count;
        goto ret;
    }

    gchar *b1, *b2;
    gssize el;

    b1 = _eventd_nd_draw_find_n_c(text, max / 2, '\n');
    b2 = _eventd_nd_draw_find_n_c(text, ( count - ( max + 1 ) / 2 + 1 ), '\n');
    el = strlen("…");

    if ( ( b2 - b1 ) < el )
    {
        gchar *tmp = text;
        *b1 = '\0';
        ++b2;
        text = g_strdup_printf("%s\n…\n%s", text, b2);
        g_free(tmp);
    }
    else
    {
        ++b1;
        strncpy(b1, "…", el);
        b1 += el;
        strncpy(b1, b2, strlen(b2) + 1);
    }

    *max_lines = 1;

ret:
    return text;
}

PangoLayout *
eventd_nd_draw_text_process(EventdNdStyle *style, EventdEvent *event, gint max_width, gint *text_height, gint *text_width)
{
    gchar *text_;
    guint8 max_lines = 0;

    text_ = _eventd_nd_draw_get_text(style, event, &max_lines);
    if ( text_ == NULL )
        return NULL;

    PangoContext *pango_context;
    PangoLayout *text;

    pango_context = pango_context_new();
    pango_context_set_font_map(pango_context, pango_cairo_font_map_get_default());

    text = pango_layout_new(pango_context);
    pango_layout_set_font_description(text, eventd_nd_style_get_text_font(style));
    pango_layout_set_alignment(text, eventd_nd_style_get_text_align(style));
    pango_layout_set_wrap(text, PANGO_WRAP_WORD_CHAR);
    pango_layout_set_ellipsize(text, PANGO_ELLIPSIZE_MIDDLE);
    pango_layout_set_width(text, max_width * PANGO_SCALE);
    if ( max_lines < 1 )
        pango_layout_set_height(text, -max_lines);
    pango_layout_set_markup(text, text_, -1);
    pango_layout_get_pixel_size(text, text_width, text_height);
    g_free(text_);

    g_object_unref(pango_context);

    return text;
}

static cairo_surface_t *
_eventd_nd_draw_limit_size(cairo_surface_t *source, gint max_width, gint max_height)
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
 * _eventd_nd_draw_get_icon_surface and alpha_mult
 * are inspired by gdk_cairo_set_source_pixbuf
 * GDK is:
 *     Copyright (C) 2011-2016 Red Hat, Inc.
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
_eventd_nd_draw_get_surface_from_pixbuf(GdkPixbuf *pixbuf)
{
    gint width, height;
    const guchar *pixels;
    gint stride;
    gboolean alpha;

    if ( pixbuf == NULL )
        return NULL;

    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    pixels = gdk_pixbuf_read_pixels(pixbuf);
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
_eventd_nd_draw_image_process(GdkPixbuf *pixbuf, EventdNdStyle *style, gint *width, gint *height)
{
    cairo_surface_t *image;

    image = _eventd_nd_draw_limit_size(_eventd_nd_draw_get_surface_from_pixbuf(pixbuf),
                                        eventd_nd_style_get_image_max_width(style),
                                        eventd_nd_style_get_image_max_height(style));

    *width = cairo_image_surface_get_width(image) + eventd_nd_style_get_image_margin(style);
    *height = cairo_image_surface_get_height(image);

    return image;
}

static cairo_surface_t *
_eventd_nd_draw_icon_process_overlay(GdkPixbuf *pixbuf, EventdNdStyle *style, gint *width, gint *height)
{
    cairo_surface_t *icon;
    gint w, h;

    icon = _eventd_nd_draw_limit_size(_eventd_nd_draw_get_surface_from_pixbuf(pixbuf),
                                        eventd_nd_style_get_icon_max_width(style),
                                        eventd_nd_style_get_icon_max_height(style));

    w = cairo_image_surface_get_width(icon);
    h = cairo_image_surface_get_height(icon);

    *width += ( w / 4 );
    *height += ( h / 4 );

    return icon;
}

static cairo_surface_t *
_eventd_nd_draw_icon_process_foreground(GdkPixbuf *pixbuf, EventdNdStyle *style, gint *width, gint *height)
{
    cairo_surface_t *icon;

    icon = _eventd_nd_draw_limit_size(_eventd_nd_draw_get_surface_from_pixbuf(pixbuf),
                                        eventd_nd_style_get_icon_max_width(style),
                                        eventd_nd_style_get_icon_max_height(style));

    gint h;

    *width += cairo_image_surface_get_width(icon) + eventd_nd_style_get_icon_margin(style);
    h = cairo_image_surface_get_height(icon);
    *height = MAX(*height, h);

    return icon;
}

static cairo_surface_t *
_eventd_nd_draw_icon_process_background(GdkPixbuf *pixbuf, EventdNdStyle *style, gint max_width, gint *width, gint *height)
{
    gint fade_width;

    fade_width = gdk_pixbuf_get_width(pixbuf) * eventd_nd_style_get_icon_fade_width(style) / 4;

    if ( ( max_width > -1 ) && ( fade_width > max_width ) )
        return NULL;

    cairo_surface_t *icon;

    icon = _eventd_nd_draw_icon_process_foreground(pixbuf, style, width, height);

    *width -= fade_width;

    return icon;
}

void
eventd_nd_draw_image_and_icon_process(EventdNdStyle *style, EventdEvent *event, gint max_width, cairo_surface_t **image, cairo_surface_t **icon, gint *text_x, gint *width, gint *height)
{
    gint load_width, load_height;
    GdkPixbuf *image_pixbuf = NULL;
    GdkPixbuf *icon_pixbuf = NULL;
    const Filename *image_filename = eventd_nd_style_get_template_image(style);
    const Filename *icon_filename = eventd_nd_style_get_template_icon(style);
    gchar *uri;

    uri = evhelpers_filename_get_uri(image_filename, event, "images");
    if ( uri != NULL )
    {
        eventd_nd_style_get_image_load_size(style, max_width, &load_width, &load_height);
        image_pixbuf = eventd_nd_pixbuf_from_uri(uri, load_width, load_height);
    }

    uri = evhelpers_filename_get_uri(icon_filename, event, "icons");
    if ( uri != NULL )
    {
        eventd_nd_style_get_icon_load_size(style, max_width, &load_width, &load_height);
        icon_pixbuf = eventd_nd_pixbuf_from_uri(uri, load_width, load_height);
    }

    *text_x = 0;
    *width = 0;
    *height = 0;
    switch ( eventd_nd_style_get_icon_placement(style) )
    {
    case EVENTD_ND_STYLE_ICON_PLACEMENT_BACKGROUND:
        if ( ( image_pixbuf != NULL )
             && ( ( max_width < 0 ) || ( gdk_pixbuf_get_width(image_pixbuf) < max_width ) ) )
        {
            *image = _eventd_nd_draw_image_process(image_pixbuf, style, width, height);
            *text_x = *width;
            max_width -= *width;
        }
        if ( ( icon_pixbuf != NULL )
             && ( ( max_width < 0 ) || ( gdk_pixbuf_get_width(icon_pixbuf) < max_width ) ) )
            *icon = _eventd_nd_draw_icon_process_background(icon_pixbuf, style, max_width, width, height);
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
                *image = _eventd_nd_draw_image_process(image_pixbuf, style, width, height);
                max_width -= *width;
            }
            if ( ( icon_pixbuf != NULL )
                 && ( ( max_width < 0 ) || ( gdk_pixbuf_get_width(icon_pixbuf) < max_width ) ) )
                *icon = _eventd_nd_draw_icon_process_overlay(icon_pixbuf, style, width, height);
            *text_x = *width;
        }
    break;
    case EVENTD_ND_STYLE_ICON_PLACEMENT_FOREGROUND:
        if ( ( image_pixbuf != NULL )
             && ( ( max_width < 0 ) || ( gdk_pixbuf_get_width(image_pixbuf) < max_width ) ) )
        {
            *image = _eventd_nd_draw_image_process(image_pixbuf, style, width, height);
            *text_x = *width;
            max_width -= *width;
        }
        if ( ( icon_pixbuf != NULL )
             && ( ( max_width < 0 ) || ( gdk_pixbuf_get_width(icon_pixbuf) < max_width ) ) )
            *icon = _eventd_nd_draw_icon_process_foreground(icon_pixbuf, style, width, height);
    break;
    }
}


static void
_eventd_nd_draw_bubble_shape(cairo_t *cr, gint radius, gint width, gint height)
{
    if ( radius < 1 )
        radius = 0;

    gint limit;

    limit = MIN(width, height) / 2;

    if ( radius > limit )
        radius = limit;

    cairo_new_path(cr);

    cairo_move_to(cr, 0, radius);
    cairo_arc(cr,
              radius, radius,
              radius,
              G_PI, 3.0 * G_PI_2);
    cairo_line_to(cr, width - radius, 0);
    cairo_arc(cr,
              width - radius, radius,
              radius,
              3.0 * G_PI_2, 0.0);
    cairo_line_to(cr, width , height - radius);
    cairo_arc(cr,
              width - radius, height - radius,
              radius,
              0.0, G_PI_2);
    cairo_line_to(cr, radius, height);
    cairo_arc(cr,
              radius, height - radius,
              radius,
              G_PI_2, G_PI);
    cairo_close_path(cr);
}


void
eventd_nd_draw_bubble_shape(cairo_t *cr, EventdNdStyle *style, gint width, gint height)
{
    gint border, radius;

    border = eventd_nd_style_get_bubble_border(style);
    radius = eventd_nd_style_get_bubble_radius(style);
    _eventd_nd_draw_bubble_shape(cr, radius, width, height);
    cairo_set_line_width(cr, border * 2);
    cairo_stroke_preserve(cr);
    cairo_fill(cr);
}

void
eventd_nd_draw_bubble_draw(cairo_t *cr, EventdNdStyle *style, gint width, gint height, gboolean shaped)
{
    gint border, radius = 0;
    Colour colour, border_colour;
    gboolean blur;

    border = eventd_nd_style_get_bubble_border(style);
    if ( shaped || ( border > 0 ) )
        radius = eventd_nd_style_get_bubble_radius(style);

    colour = eventd_nd_style_get_bubble_colour(style);
    border_colour = eventd_nd_style_get_bubble_border_colour(style);

    _eventd_nd_draw_bubble_shape(cr, radius, width, height);
    cairo_set_source_rgba(cr, border_colour.r, border_colour.g, border_colour.b, border_colour.a);

    blur = shaped && eventd_nd_style_get_bubble_border_blur(style);
    if ( blur )
    {
        cairo_fill_preserve(cr);
        blur = eventd_nd_draw_blur_surface(cr, border);
        /* Fallback to plain border if blur failed */
    }

    if ( ! blur )
    {
        if ( ( ! shaped ) && ( border > 0 ) )
            cairo_paint(cr);
        else
        {
            cairo_set_line_width(cr, border * 2);
            cairo_stroke_preserve(cr);
        }
    }
    cairo_set_source_rgba(cr, colour.r, colour.g, colour.b, colour.a);
    cairo_fill(cr);
}

void
eventd_nd_draw_text_draw(cairo_t *cr, EventdNdStyle *style, PangoLayout *text, gint offset_x, gint max_height)
{
    Colour colour;

    colour = eventd_nd_style_get_text_colour(style);
    cairo_set_source_rgba(cr, colour.r, colour.g, colour.b, colour.a);
    cairo_new_path(cr);
    cairo_move_to(cr, offset_x, 0);
    pango_cairo_update_layout(cr, text);
    pango_cairo_layout_path(cr, text);
    cairo_fill(cr);
}

static gint
_eventd_nd_draw_get_valign(EventdNdAnchorVertical anchor, gint height, gint surface_height)
{
    switch ( anchor )
    {
    case EVENTD_ND_VANCHOR_TOP:
        return 0;
    case EVENTD_ND_VANCHOR_CENTER:
        return ( height / 2 ) - (surface_height / 2);
    case EVENTD_ND_VANCHOR_BOTTOM:
        return height - surface_height;
    }
    g_assert_not_reached();
}

static void
_eventd_nd_draw_surface_draw(cairo_t *cr, cairo_surface_t *surface, gint x, gint y)
{
    cairo_set_source_surface(cr, surface, x, y);
    cairo_rectangle(cr, x, y, cairo_image_surface_get_width(surface), cairo_image_surface_get_height(surface));
    cairo_fill(cr);
}

static void
_eventd_nd_draw_image_draw(cairo_t *cr, cairo_surface_t *image, EventdNdStyle *style, gint width, gint height)
{
    gint x, y;

    x = 0;
    y = _eventd_nd_draw_get_valign(eventd_nd_style_get_image_anchor(style), height, cairo_image_surface_get_height(image));
    _eventd_nd_draw_surface_draw(cr, image, x, y);
}

static void
_eventd_nd_draw_image_and_icon_draw_overlay(cairo_t *cr, cairo_surface_t *image, cairo_surface_t *icon, EventdNdStyle *style, gint width, gint height)
{
    if ( image == NULL )
        return;

    if ( icon == NULL )
        _eventd_nd_draw_image_draw(cr, image, style, width, height);
    else
    {
        gint image_x, image_y;
        gint icon_x, icon_y;
        gint w, h;

        w = cairo_image_surface_get_width(icon);
        h =  cairo_image_surface_get_height(icon);

        image_x = _eventd_nd_draw_get_valign(eventd_nd_style_get_image_anchor(style), height, cairo_image_surface_get_height(image));;
        image_y = 0;
        icon_x = image_x + cairo_image_surface_get_width(image) - ( 3 * w / 4 );
        icon_y = image_y + cairo_image_surface_get_height(image) - ( 3 * h / 4 );

        _eventd_nd_draw_surface_draw(cr, image, image_x, image_y);
        _eventd_nd_draw_surface_draw(cr, icon, icon_x, icon_y);
    }
}

static void
_eventd_nd_draw_image_and_icon_draw_foreground(cairo_t *cr, cairo_surface_t *image, cairo_surface_t *icon, EventdNdStyle *style, gint width, gint height)
{
    if ( image != NULL )
        _eventd_nd_draw_image_draw(cr, image, style, width, height);

    if ( icon != NULL )
    {
        gint x, y;

        x = width - cairo_image_surface_get_width(icon);
        y = _eventd_nd_draw_get_valign(eventd_nd_style_get_icon_anchor(style), height, cairo_image_surface_get_height(icon));
        _eventd_nd_draw_surface_draw(cr, icon, x, y);
    }
}

static void
_eventd_nd_draw_image_and_icon_draw_background(cairo_t *cr, cairo_surface_t *image, cairo_surface_t *icon, EventdNdStyle *style, gint width, gint height)
{
    if ( image != NULL )
        _eventd_nd_draw_image_draw(cr, image, style, width, height);

    if ( icon != NULL )
    {
        gint x1, x2, y;
        cairo_pattern_t *mask;

        x2 = width;
        x1 = x2 - cairo_image_surface_get_width(icon);
        y = _eventd_nd_draw_get_valign(eventd_nd_style_get_icon_anchor(style), height, cairo_image_surface_get_height(icon));

        mask = cairo_pattern_create_linear(x1, 0, x2, 0);
        cairo_pattern_add_color_stop_rgba(mask, 0, 0, 0, 0, 0);
        cairo_pattern_add_color_stop_rgba(mask, eventd_nd_style_get_icon_fade_width(style), 0, 0, 0, 1);

        cairo_set_source_surface(cr, icon, x1, y);
        cairo_mask(cr, mask);
    }
}

void
eventd_nd_draw_image_and_icon_draw(cairo_t *cr, cairo_surface_t *image, cairo_surface_t *icon, EventdNdStyle *style, gint width, gint height)
{
    switch ( eventd_nd_style_get_icon_placement(style) )
    {
    case EVENTD_ND_STYLE_ICON_PLACEMENT_BACKGROUND:
        _eventd_nd_draw_image_and_icon_draw_background(cr, image, icon, style, width, height);
    break;
    case EVENTD_ND_STYLE_ICON_PLACEMENT_OVERLAY:
        _eventd_nd_draw_image_and_icon_draw_overlay(cr, image, icon, style, width, height);
    break;
    case EVENTD_ND_STYLE_ICON_PLACEMENT_FOREGROUND:
        _eventd_nd_draw_image_and_icon_draw_foreground(cr, image, icon, style, width, height);
    break;
    }
}
