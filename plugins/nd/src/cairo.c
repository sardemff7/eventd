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

#include <math.h>

#include <string.h>
#include <glib.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>

#include <libeventd-event-types.h>
#include <libeventd-nd-notification.h>

#include "style.h"

#include "cairo.h"

static GRegex *regex_amp = NULL;
static GRegex *regex_markup = NULL;

void
eventd_nd_cairo_init()
{
    GError *error = NULL;

    if ( regex_amp == NULL )
    {
        regex_amp = g_regex_new("&(?!amp;|quot;|apos;|lt;|gt;)", G_REGEX_OPTIMIZE, 0, &error);
        if ( regex_amp == NULL )
            g_warning("Couldn't create amp regex: %s", error->message);
        g_clear_error(&error);
    }
    else
        g_regex_ref(regex_amp);

    if ( regex_markup == NULL )
    {
        regex_markup = g_regex_new("<(?!/?[biu]>)", G_REGEX_OPTIMIZE, 0, &error);
        if ( regex_markup == NULL )
            g_warning("Couldn't create markup regex: %s", error->message);
        g_clear_error(&error);
    }
    else
        g_regex_ref(regex_markup);
}

void
eventd_nd_cairo_uninit()
{
    if ( regex_markup != NULL )
        g_regex_unref(regex_markup);
    if ( regex_amp != NULL )
        g_regex_unref(regex_amp);
}

struct _EventdNdBubble {
    GList *surfaces;
};

static gchar *
_eventd_nd_cairo_message_escape(const gchar *message)
{
    GError *error = NULL;
    gchar *escaped, *tmp = NULL;

    escaped = g_regex_replace_literal(regex_amp, message, -1, 0, "&amp;" , 0, &error);
    if ( escaped == NULL )
    {
        g_warning("Couldn't escape amp: %s", error->message);
        goto fallback;
    }

    escaped = g_regex_replace_literal(regex_markup, tmp = escaped, -1, 0, "&lt;" , 0, &error);
    if ( escaped == NULL )
    {
        g_warning("Couldn't escape markup: %s", error->message);
        goto fallback;
    }
    g_free(tmp);

    if ( ! pango_parse_markup(tmp = escaped, -1, 0, NULL, NULL, NULL, NULL) )
        goto fallback;

    return escaped;

fallback:
    g_free(tmp);
    g_clear_error(&error);
    return g_markup_escape_text(message, -1);
}

static gchar *
_eventd_nd_cairo_get_message(const gchar *message, guint8 max)
{
    gchar *escaped;
    gchar **message_lines;
    guint size;
    gchar *ret;

    escaped = _eventd_nd_cairo_message_escape(message);
    message_lines = g_strsplit(escaped, "\n", -1);
    g_free(escaped);

    size = g_strv_length(message_lines);

    if ( size > max )
    {
        gchar **tmp = message_lines;
        gchar **message_line;
        gchar **message_new_line;

        message_lines = g_new(gchar *, max + 1);
        message_lines[max] = NULL;

        for ( message_line = tmp + size - ( max - 2 ), message_new_line = message_lines ; *message_line != NULL ; ++message_line )
        {
            *message_new_line = *message_line;
            *message_line = NULL;
        }

        g_strfreev(tmp);
    }

    ret = g_strjoinv("\n", message_lines);

    g_strfreev(message_lines);

    return ret;
}

static void
_eventd_nd_cairo_text_process(LibeventdNdNotification *notification, EventdNdStyle *style, PangoLayout **title, PangoLayout **message, gint *text_height, gint *text_width)
{
    PangoContext *pango_context;

    pango_context = pango_context_new();
    pango_context_set_font_map(pango_context, pango_cairo_font_map_get_default());

    *title = pango_layout_new(pango_context);
    pango_layout_set_font_description(*title, eventd_nd_style_get_title_font(style));
    pango_layout_set_text(*title, libeventd_nd_notification_get_title(notification), -1);
    pango_layout_get_pixel_size(*title, text_width, text_height);

    const gchar *tmp;
    tmp = libeventd_nd_notification_get_message(notification);
    if ( tmp != NULL )
    {
        gchar *text;
        gint w;
        gint h;

        text = _eventd_nd_cairo_get_message(tmp, eventd_nd_style_get_message_max_lines(style));

        *message = pango_layout_new(pango_context);
        pango_layout_set_font_description(*message, eventd_nd_style_get_message_font(style));
        pango_layout_set_height(*message, -eventd_nd_style_get_message_max_lines(style));
        pango_layout_set_markup(*message, text, -1);
        pango_layout_get_pixel_size(*message, &w, &h);

        g_free(text);

        *text_height += eventd_nd_style_get_message_spacing(style) + h;
        *text_width = MAX(*text_width, w);
    }

    g_object_unref(pango_context);
}

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
 *     Copyright (C) 2005 Red Hat, Inc.
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

    *width = cairo_image_surface_get_width(image);
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

    *width += ( w / 2 );
    *height += ( h / 2 );

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
_eventd_nd_cairo_icon_process_background(GdkPixbuf *pixbuf, EventdNdStyle *style, gint *width, gint *height)
{
    cairo_surface_t *icon;

    icon = _eventd_nd_cairo_icon_process_foreground(pixbuf, style, width, height);

    *width -= cairo_image_surface_get_width(icon) * eventd_nd_style_get_icon_fade_width(style) / 4;

    return icon;
}

static void
_eventd_nd_cairo_image_and_icon_process(LibeventdNdNotification *notification, EventdNdStyle *style, cairo_surface_t **image, cairo_surface_t **icon, gint *text_margin, gint *width, gint *height)
{
    GdkPixbuf *image_pixbuf;
    GdkPixbuf *icon_pixbuf;

    image_pixbuf = libeventd_nd_notification_get_image(notification);
    icon_pixbuf = libeventd_nd_notification_get_icon(notification);

    switch ( eventd_nd_style_get_icon_placement(style) )
    {
    case EVENTD_ND_STYLE_ICON_PLACEMENT_BACKGROUND:
        if ( image_pixbuf != NULL )
        {
            *image = _eventd_nd_cairo_image_process(image_pixbuf, style, width, height);
            *text_margin = *width + eventd_nd_style_get_image_margin(style);
        }
        if ( icon_pixbuf != NULL )
            *icon = _eventd_nd_cairo_icon_process_background(icon_pixbuf, style, width, height);
    break;
    case EVENTD_ND_STYLE_ICON_PLACEMENT_OVERLAY:
        if ( image_pixbuf != NULL )
        {
            *image = _eventd_nd_cairo_image_process(image_pixbuf, style, width, height);
            if ( icon_pixbuf != NULL )
                *icon = _eventd_nd_cairo_icon_process_overlay(icon_pixbuf, style, width, height);
        }
        else if ( icon_pixbuf != NULL )
            *image = _eventd_nd_cairo_image_process(icon_pixbuf, style, width, height);
        if ( *image != NULL )
            *text_margin = *width + eventd_nd_style_get_image_margin(style);
    break;
    case EVENTD_ND_STYLE_ICON_PLACEMENT_FOREGROUND:
        if ( image_pixbuf != NULL )
        {
            *image = _eventd_nd_cairo_image_process(image_pixbuf, style, width, height);
            *text_margin = *width + eventd_nd_style_get_image_margin(style);
        }
        if ( icon_pixbuf != NULL )
            *icon = _eventd_nd_cairo_icon_process_foreground(icon_pixbuf, style, width, height);
    break;
    }
}

static void
_eventd_nd_cairo_shape_draw(cairo_t *cr, gint radius, gint width, gint height)
{
    gint limit;

    limit = MIN(width, height);

    if ( ( radius * 2 ) > limit )
        radius = (gdouble) limit / 2.;

    cairo_new_path(cr);

    cairo_move_to(cr, 0, radius);
    cairo_arc(cr,
              radius, radius,
              radius,
              M_PI, 3.0 * M_PI / 2.0);
    cairo_line_to(cr, width-radius, 0);
    cairo_arc(cr,
              width-radius, radius,
              radius,
              3.0 * M_PI / 2.0, 0.0);
    cairo_line_to(cr, width, height-radius);
    cairo_arc(cr,
              width-radius, height-radius,
              radius,
              0.0, M_PI / 2.0);
    cairo_line_to(cr, radius, height);
    cairo_arc(cr,
              radius, height-radius,
              radius,
              M_PI / 2.0, M_PI);
    cairo_close_path(cr);

    cairo_fill(cr);
}

static void
_eventd_nd_cairo_bubble_draw(cairo_t *cr, Colour colour, gint width, gint height)
{
    cairo_set_source_rgba(cr, colour.r, colour.g, colour.b, colour.a);

    cairo_new_path(cr);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);
}

static void
_eventd_nd_cairo_text_draw(cairo_t *cr, EventdNdStyle *style, PangoLayout *title, PangoLayout *message, gint offset_x, gint offset_y, gint max_width)
{
    Colour colour;

    colour = eventd_nd_style_get_title_colour(style);
    cairo_set_source_rgba(cr, colour.r, colour.g, colour.b, colour.a);
    cairo_new_path(cr);
    cairo_move_to(cr, offset_x, offset_y);
    pango_layout_set_width(title, max_width * PANGO_SCALE);
    pango_cairo_update_layout(cr, title);
    pango_cairo_layout_path(cr, title);
    cairo_fill(cr);

    if ( message != NULL )
    {
        gint h;
        pango_layout_get_pixel_size(title, NULL, &h);

        offset_y += eventd_nd_style_get_message_spacing(style) + h;

        colour = eventd_nd_style_get_message_colour(style);
        cairo_set_source_rgba(cr, colour.r, colour.g, colour.b, colour.a);
        cairo_new_path(cr);
        cairo_move_to(cr, offset_x, offset_y);
        pango_layout_set_width(message, max_width * PANGO_SCALE);
        pango_cairo_update_layout(cr, message);
        pango_cairo_layout_path(cr, message);
        cairo_fill(cr);

        g_object_unref(message);
    }

    g_object_unref(title);
}

static gint
_eventd_nd_cairo_get_valign(EventdNdAnchor anchor, gint height, gint padding, gint surface_height)
{
    switch ( anchor )
    {
    case EVENTD_ND_ANCHOR_TOP:
        return padding;
    case EVENTD_ND_ANCHOR_VCENTER:
        return ( height / 2 ) - (surface_height / 2);
    case EVENTD_ND_ANCHOR_BOTTOM:
        return height - padding - surface_height;
    default:
        g_assert_not_reached();
    }
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
_eventd_nd_cairo_image_and_icon_draw_overlay(cairo_t *cr, cairo_surface_t *image, cairo_surface_t *icon, EventdNdStyle *style)
{
    if ( image == NULL )
        return;

    gint padding;

    padding = eventd_nd_style_get_bubble_padding(style);

    if ( icon == NULL )
        _eventd_nd_cairo_surface_draw(cr, image, padding, padding);
    else
    {
        gint image_x, image_y;
        gint icon_x, icon_y;
        gint w, h;

        w = cairo_image_surface_get_width(icon);
        h =  cairo_image_surface_get_height(icon);

        image_x = padding + w / 4;
        image_y = padding + h / 4;
        icon_x = padding + cairo_image_surface_get_width(image) - ( w / 2 );
        icon_y = padding + cairo_image_surface_get_height(image) - ( h / 2 );

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
        _eventd_nd_cairo_surface_draw(cr, image, padding, padding);

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
        _eventd_nd_cairo_surface_draw(cr, image, padding, padding);

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

static void
_eventd_nd_cairo_image_and_icon_draw(cairo_t *cr, cairo_surface_t *image, cairo_surface_t *icon, EventdNdStyle *style, gint width, gint height)
{
    switch ( eventd_nd_style_get_icon_placement(style) )
    {
    case EVENTD_ND_STYLE_ICON_PLACEMENT_BACKGROUND:
        _eventd_nd_cairo_image_and_icon_draw_background(cr, image, icon, style, width, height);
    break;
    case EVENTD_ND_STYLE_ICON_PLACEMENT_OVERLAY:
        _eventd_nd_cairo_image_and_icon_draw_overlay(cr, image, icon, style);
    break;
    case EVENTD_ND_STYLE_ICON_PLACEMENT_FOREGROUND:
        _eventd_nd_cairo_image_and_icon_draw_foreground(cr, image, icon, style, width, height);
    break;
    }
}

void
eventd_nd_cairo_get_surfaces(EventdEvent *event, LibeventdNdNotification *notification, EventdNdStyle *style, cairo_surface_t **bubble, cairo_surface_t **shape)
{
    gint padding;
    gint min_width, max_width;

    gint width;
    gint height;

    PangoLayout *title;
    PangoLayout *message = NULL;
    cairo_surface_t *image = NULL;
    cairo_surface_t *icon = NULL;

    gint text_width = 0, text_height = 0;
    gint text_margin = 0;
    gint image_width = 0, image_height = 0;

    cairo_t *cr;

    /* proccess data */
    _eventd_nd_cairo_text_process(notification, style, &title, &message, &text_height, &text_width);

    _eventd_nd_cairo_image_and_icon_process(notification, style, &image, &icon, &text_margin, &image_width, &image_height);

    /* compute the bubble size */
    padding = eventd_nd_style_get_bubble_padding(style);
    min_width = eventd_nd_style_get_bubble_min_width(style);
    max_width = eventd_nd_style_get_bubble_max_width(style);

    width = 2 * padding + image_width + text_width;

    if ( min_width > -1 )
        width = MAX(width, min_width);
    if ( max_width > -1 )
        width = MIN(width, max_width);

    height = 2 * padding + MAX(image_height, text_height);

    /* draw the bubble */
    *bubble = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cr = cairo_create(*bubble);
    _eventd_nd_cairo_bubble_draw(cr, eventd_nd_style_get_bubble_colour(style), width, height);
    _eventd_nd_cairo_image_and_icon_draw(cr, image, icon, style, width, height);
    _eventd_nd_cairo_text_draw(cr, style, title, message, padding + text_margin, padding, text_width);
    cairo_destroy(cr);

    *shape = cairo_image_surface_create(CAIRO_FORMAT_A1, width, height);
    cr = cairo_create(*shape);
    cairo_set_source_rgba(cr, 0, 0, 0, 1);
    _eventd_nd_cairo_shape_draw(cr, eventd_nd_style_get_bubble_radius(style), width, height);
    cairo_destroy(cr);
}
