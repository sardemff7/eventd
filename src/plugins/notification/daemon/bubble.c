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

#include "../notification.h"
#include "../icon.h"

#include "types.h"
#include "daemon.h"

#include "style.h"
#include "style-internal.h"
#include "backends/backend.h"

#include "bubble.h"

static GRegex *regex_amp = NULL;
static GRegex *regex_markup = NULL;

void
eventd_nd_bubble_init()
{
    GError *error = NULL;

    if ( regex_amp == NULL )
    {
        regex_amp = g_regex_new("&(?!amp;|quot;|apos;|lt;|gt;)", G_REGEX_OPTIMIZE, 0, &error);
        if ( regex_amp == NULL )
            g_warning("Couldn’t create amp regex: %s", error->message);
        g_clear_error(&error);
    }
    else
        g_regex_ref(regex_amp);

    if ( regex_markup == NULL )
    {
        regex_markup = g_regex_new("<(?!/?[biu]>)", G_REGEX_OPTIMIZE, 0, &error);
        if ( regex_markup == NULL )
            g_warning("Couldn’t create markup regex: %s", error->message);
        g_clear_error(&error);
    }
    else
        g_regex_ref(regex_markup);
}

void
eventd_nd_bubble_uninit()
{
    if ( regex_markup != NULL )
        g_regex_unref(regex_markup);
    if ( regex_amp != NULL )
        g_regex_unref(regex_amp);
}

typedef struct {
    gchar *text;
    PangoLayout *layout;
    gint height;
} EventdNdTextLine;

struct _EventdNdBubble {
    GList *surfaces;
};

static EventdNdTextLine *
_eventd_nd_bubble_text_line_new(const gchar *text)
{
    EventdNdTextLine *line;

    line = g_new0(EventdNdTextLine, 1);
    line->text = g_strdup(text);

    return line;
}

static void
_eventd_nd_bubble_text_line_free(gpointer data)
{
    EventdNdTextLine *line = data;

    g_object_unref(line->layout);
    g_free(line->text);

    g_free(line);
}

static gchar **
_eventd_nd_bubble_message_escape_and_split(const gchar *message)
{
    GError *error = NULL;
    gchar *escaped, *tmp = NULL;
    gchar **ret;

    escaped = g_regex_replace_literal(regex_amp, message, -1, 0, "&amp;" , 0, &error);
    if ( escaped == NULL )
    {
        g_warning("Couldn’t escape amp: %s", error->message);
        goto fallback;
    }

    escaped = g_regex_replace_literal(regex_markup, tmp = escaped, -1, 0, "&lt;" , 0, &error);
    if ( escaped == NULL )
    {
        g_warning("Couldn’t escape markup: %s", error->message);
        goto fallback;
    }

    if ( ! pango_parse_markup(tmp = escaped, -1, 0, NULL, NULL, NULL, NULL) )
        goto fallback;

split:
    ret = g_strsplit(escaped, "\n", -1);
    g_free(escaped);

    return ret;

fallback:
    g_free(tmp);
    g_clear_error(&error);
    escaped = g_markup_escape_text(message, -1);
    goto split;
}

static GList *
_eventd_nd_bubble_text_split(EventdNotificationNotification *notification, EventdNdStyle *style)
{
    GList *lines = NULL;
    gchar *escaped_title;
    gchar **message_lines;
    gchar **message_line;
    guint8 max;
    guint size;

    escaped_title = g_markup_escape_text(notification->title, -1);
    lines = g_list_prepend(lines, _eventd_nd_bubble_text_line_new(escaped_title));
    g_free(escaped_title);

    message_lines = _eventd_nd_bubble_message_escape_and_split(notification->message);

    max = style->message_max_lines;
    size = g_strv_length(message_lines);

    if ( size > max )
    {
        lines = g_list_prepend(lines, _eventd_nd_bubble_text_line_new(*message_lines));
        lines = g_list_prepend(lines, _eventd_nd_bubble_text_line_new("[…]"));
        message_line = message_lines + size - ( max - 2 );
    }
    else
        message_line = message_lines;

    for ( ; *message_line != NULL ; ++message_line )
        lines = g_list_prepend(lines, _eventd_nd_bubble_text_line_new(*message_line));

    g_strfreev(message_lines);

    return g_list_reverse(lines);
}

static void
_eventd_nd_bubble_text_process_line(EventdNdTextLine *line, PangoContext *pango_context, PangoFontDescription *font, gint *text_height, gint *text_width)
{
    gint w;

    line->layout = pango_layout_new(pango_context);
    pango_layout_set_font_description(line->layout, font);
    pango_layout_set_markup(line->layout, line->text, -1);
    pango_layout_get_pixel_size(line->layout, &w, &line->height);

    *text_height += line->height;

    if ( w > *text_width )
        *text_width = w;
}

static GList *
_eventd_nd_bubble_text_process(EventdNotificationNotification *notification, EventdNdStyle *style, gint *text_height, gint *text_width)
{
    GList *ret;
    GList *lines = NULL;
    EventdNdTextLine *line;
    PangoContext *pango_context;

    lines = _eventd_nd_bubble_text_split(notification, style);

    *text_height = 0;
    *text_width = 0;

    pango_context = style->pango_context;
    pango_context_set_font_map(pango_context, pango_cairo_font_map_get_default());

    ret = g_list_first(lines);
    line = lines->data;
    _eventd_nd_bubble_text_process_line(line, pango_context, style->title_font, text_height, text_width);

    lines = g_list_next(lines);
    if ( lines != NULL )
    {
        gint spacing;
        PangoFontDescription *font;

        spacing = style->message_spacing;
        line->height += spacing;
        *text_height += spacing;

        font = style->message_font;
        for ( ; lines != NULL ; lines = g_list_next(lines) )
            _eventd_nd_bubble_text_process_line(lines->data, pango_context, font, text_height, text_width);
    }

    return ret;
}

static gdouble
_eventd_nd_bubble_limit_icon_size(EventdNdStyle *style, gint width, gint height, gint *r_width, gint *r_height)
{
    gdouble max_width, max_height;
    gdouble s = 1.0;

    max_width = style->icon_max_width;
    max_height = style->icon_max_height;

    if ( ( width > max_width ) && ( height > max_height ) )
    {
        if ( width > height )
            s = max_width / width;
        else
            s = max_height / height;
    }
    else if ( width > max_width )
        s = max_width / width;
    else if ( height > max_height )
        s = max_height / height;

    (*r_width) = width * s;
    (*r_height) = height * s;

    return s;
}

/*
 * _eventd_nd_bubble_get_icon_surface and alpha_mult
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
_eventd_nd_bubble_get_icon_surface_from_data(gint width, gint height, const guchar *pixels, gint stride, gboolean alpha)
{
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
_eventd_nd_bubble_get_icon_surface(const guchar *data, gsize length, gchar *format)
{
    GdkPixbuf *pixbuf = NULL;
    cairo_surface_t *r;
    gint width, height;
    const guchar *pixels;
    gint stride;
    gboolean alpha;

    if ( format != NULL )
    {
        width = g_ascii_strtoll(format, &format, 16);
        height = g_ascii_strtoll(format+1, &format, 16);
        stride = g_ascii_strtoll(format+1, &format, 16);
        alpha = g_ascii_strtoll(format+1, &format, 16);

        pixels = data;
    }
    else
    {
        pixbuf = eventd_notification_icon_get_pixbuf_from_data(data, length, 0);
        if ( pixbuf == NULL )
            return NULL;

        width = gdk_pixbuf_get_width(pixbuf);
        height = gdk_pixbuf_get_height(pixbuf);
        pixels = gdk_pixbuf_get_pixels(pixbuf);
        stride = gdk_pixbuf_get_rowstride(pixbuf);
        alpha = gdk_pixbuf_get_has_alpha(pixbuf);
    }

    r = _eventd_nd_bubble_get_icon_surface_from_data(width, height, pixels, stride, alpha);
    if ( pixbuf != NULL )
        g_object_unref(pixbuf);

    return r;
}

static cairo_surface_t *
_eventd_nd_bubble_icon_process(EventdNdStyle *style, cairo_surface_t *icon, cairo_surface_t *overlay_icon, gint *icon_width, gint *icon_height)
{
    gint w, h;

    gdouble scale;
    cairo_surface_t *surface;
    cairo_t *cr;
    cairo_pattern_t *pattern;
    cairo_matrix_t matrix;

    *icon_width = 0;
    *icon_height = 0;

    if ( icon == NULL )
    {
        if ( overlay_icon == NULL )
            return NULL;
        else
            return _eventd_nd_bubble_icon_process(style, overlay_icon, NULL, icon_width, icon_height);
    }

    w = cairo_image_surface_get_width(icon);
    h = cairo_image_surface_get_height(icon);

    scale = _eventd_nd_bubble_limit_icon_size(style, w, h, icon_width, icon_height);

    if ( scale != 1.0 )
    {
        surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
        cr = cairo_create(surface);

        cairo_matrix_init_scale(&matrix, 1. / scale, 1. / scale);

        pattern = cairo_pattern_create_for_surface(icon);
        cairo_pattern_set_matrix(pattern, &matrix);

        cairo_set_source(cr, pattern);
        cairo_rectangle(cr, 0, 0, w, h);
        cairo_fill(cr);

        cairo_pattern_destroy(pattern);
        cairo_destroy(cr);
        cairo_surface_destroy(icon);

        icon = surface;
    }
    else
        surface = icon;

    if ( overlay_icon != NULL )
    {
        gdouble overlay_scale;
        overlay_scale = style->overlay_scale;

        w = cairo_image_surface_get_width(overlay_icon);
        h = cairo_image_surface_get_height(overlay_icon);
        if ( w > h )
            scale = ( ( (gdouble) *icon_width * overlay_scale ) / (gdouble) w );
        else
            scale = ( ( (gdouble) *icon_height * overlay_scale ) / (gdouble) h );

        w *= scale;
        h *= scale;

        *icon_width +=  w / 2;
        *icon_height += h / 2;

        surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, *icon_width, *icon_height);

        cr = cairo_create(surface);

        cairo_set_source_surface(cr, icon, w / 4, h / 4);
        cairo_rectangle(cr, w / 4, h / 4, *icon_width, *icon_height);
        cairo_fill(cr);

        cairo_matrix_init_scale(&matrix, 1. / scale, 1. / scale);
        cairo_matrix_translate(&matrix, - *icon_width + w, - *icon_height + h);

        pattern = cairo_pattern_create_for_surface(overlay_icon);
        cairo_pattern_set_matrix(pattern, &matrix);

        cairo_set_source(cr, pattern);
        cairo_rectangle(cr, 0, 0, *icon_width, *icon_height);
        cairo_fill(cr);

        cairo_pattern_destroy(pattern);
        cairo_destroy(cr);

        cairo_surface_destroy(overlay_icon);
        cairo_surface_destroy(icon);
    }

    *icon_width += style->icon_spacing;

    return surface;
}


static void
_eventd_nd_bubble_shape_draw(cairo_t *cr, gint radius, gint width, gint height)
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
_eventd_nd_bubble_bubble_draw(cairo_t *cr, Colour colour, gint width, gint height)
{
    cairo_set_source_rgba(cr, colour.r, colour.g, colour.b, colour.a);

    cairo_new_path(cr);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);
}

static void
_eventd_nd_bubble_text_draw_line(EventdNdTextLine *line, cairo_t *cr, gint offset_x, gint *offset_y)
{
    cairo_new_path(cr);

    cairo_move_to(cr, offset_x, *offset_y);
    pango_cairo_update_layout(cr, line->layout);
    pango_cairo_layout_path(cr, line->layout);

    cairo_fill(cr);

    *offset_y += line->height;
}

static void
_eventd_nd_bubble_text_draw(cairo_t *cr, GList *lines, EventdNdStyle *style, gint offset_x, gint offset_y)
{
    Colour colour;

    colour = style->title_colour;
    cairo_set_source_rgba(cr, colour.r, colour.g, colour.b, colour.a);

    lines = g_list_first(lines);
    _eventd_nd_bubble_text_draw_line(lines->data, cr, offset_x, &offset_y);

    colour = style->message_colour;
    cairo_set_source_rgba(cr, colour.r, colour.g, colour.b, colour.a);
    for ( lines = g_list_next(lines) ; lines != NULL ; lines = g_list_next(lines) )
        _eventd_nd_bubble_text_draw_line(lines->data, cr, offset_x, &offset_y);

    g_list_free_full(lines, _eventd_nd_bubble_text_line_free);
}

static void
_eventd_nd_bubble_icon_draw(cairo_t *cr, cairo_surface_t *icon, gint x, gint y)
{
    if ( icon == NULL )
        return;

    cairo_set_source_surface(cr, icon, x, y);
    cairo_rectangle(cr, x, y, cairo_image_surface_get_width(icon), cairo_image_surface_get_height(icon));
    cairo_fill(cr);

    cairo_surface_destroy(icon);
}

EventdNdBubble *
eventd_nd_bubble_new(EventdEvent *event, EventdNotificationNotification *notification, EventdNdStyle *style, GList *displays)
{
    gint padding;
    gint min_width, max_width;

    gint width;
    gint height;

    cairo_surface_t *icon = NULL;

    GList *lines = NULL;
    gint text_width = 0, text_height = 0;
    gint icon_width = 0, icon_height = 0;

    cairo_surface_t *bubble;
    cairo_surface_t *shape;
    cairo_t *cr;

    EventdNdBubble *bubble_surfaces;
    GList *display;

    bubble_surfaces = g_new0(EventdNdBubble, 1);

    /* proccess data */
    lines = _eventd_nd_bubble_text_process(notification, style, &text_height, &text_width);

    icon = _eventd_nd_bubble_icon_process(style,
                                          _eventd_nd_bubble_get_icon_surface(notification->icon, notification->icon_length, notification->icon_format),
                                          _eventd_nd_bubble_get_icon_surface(notification->overlay_icon, notification->overlay_icon_length, notification->overlay_icon_format),
                                          &icon_width, &icon_height);

    /* compute the bubble size */
    padding = style->bubble_padding;
    min_width = style->bubble_min_width;
    max_width = style->bubble_max_width;

    width = 2 * padding + icon_width + text_width;

    if ( min_width > -1 )
        width = MAX(width, min_width);
    if ( max_width > -1 )
        width = MIN(width, max_width);

    height = 2 * padding + MAX(icon_height, text_height);

    /* draw the bubble */
    bubble = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cr = cairo_create(bubble);
    _eventd_nd_bubble_bubble_draw(cr, style->bubble_colour, width, height);
    _eventd_nd_bubble_text_draw(cr, lines, style, padding + icon_width, padding);
    _eventd_nd_bubble_icon_draw(cr, icon, padding, padding);
    cairo_destroy(cr);

    shape = cairo_image_surface_create(CAIRO_FORMAT_A1, width, height);
    cr = cairo_create(shape);
    cairo_set_source_rgba(cr, 0, 0, 0, 1);
    _eventd_nd_bubble_shape_draw(cr, style->bubble_radius, width, height);
    cairo_destroy(cr);

    for ( display = displays ; display != NULL ; display = g_list_next(display) )
    {
        EventdNdDisplayContext *display_ = display->data;
        EventdNdSurface *surface;
        surface = display_->backend->surface_new(event, display_->display, width, height, bubble, shape);
        if ( surface != NULL )
        {
            EventdNdSurfaceContext *surface_context;

            surface_context = g_new(EventdNdSurfaceContext, 1);
            surface_context->backend = display_->backend;
            surface_context->surface = surface;

            bubble_surfaces->surfaces = g_list_prepend(bubble_surfaces->surfaces, surface_context);
        }
    }

    cairo_surface_destroy(shape);
    cairo_surface_destroy(bubble);

    return bubble_surfaces;
}

void
eventd_nd_bubble_show(EventdNdBubble *bubble)
{
    GList *surface_;

    for ( surface_ = bubble->surfaces ; surface_ != NULL ; surface_ = g_list_next(surface_) )
    {
        EventdNdSurfaceContext *surface = surface_->data;
        surface->backend->surface_show(surface->surface);
    }
}

void
eventd_nd_bubble_hide(EventdNdBubble *bubble)
{
    GList *surface_;

    for ( surface_ = bubble->surfaces ; surface_ != NULL ; surface_ = g_list_next(surface_) )
    {
        EventdNdSurfaceContext *surface = surface_->data;
        surface->backend->surface_hide(surface->surface);
    }
}

static void
_eventd_nd_bubble_surface_free(gpointer data)
{
    EventdNdSurfaceContext *surface = data;

    surface->backend->surface_free(surface->surface);

    g_free(surface);
}

void
eventd_nd_bubble_free(gpointer data)
{
    EventdNdBubble *bubble = data;

    g_list_free_full(bubble->surfaces, _eventd_nd_bubble_surface_free);

    g_free(bubble);
}
