/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2018 Quentin "Sardem FF7" Glidic
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

#include <string.h>

#include <glib.h>
#include <glib-object.h>

#include <nkutils-xdg-theme.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>

#include "libeventd-event.h"
#include "libeventd-helpers-config.h"

#include "style.h"
#include "pixbuf.h"
#include "blur.h"

#include "draw.h"


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

static void
_eventd_nd_draw_text_parser_start_element(GMarkupParseContext *context, const gchar *element_name, const gchar **attribute_names, const gchar **attribute_values, gpointer user_data, GError **error)
{
    GString *string = user_data;

    if ( g_strcmp0(element_name, "markup") == 0 )
        return;
    if ( g_strcmp0(element_name, "a") == 0 )
        return;

    g_string_append(g_string_append_c(string, '<'), element_name);

    gsize i;
    for ( i = 0 ; attribute_names[i] != NULL ; ++i )
    {
        g_string_append_c(g_string_append(g_string_append_c(string, ' '), attribute_names[i]), '=');
        if ( g_utf8_strchr(attribute_values[i], -1, '"') == NULL )
            g_string_append_c(g_string_append(g_string_append_c(string, '"'), attribute_values[i]), '"');
        else if ( g_utf8_strchr(attribute_values[i], -1, '\'') == NULL )
            g_string_append_c(g_string_append(g_string_append_c(string, '\''), attribute_values[i]), '\'');
    }

    g_string_append_c(string, '>');
}

static void
_eventd_nd_draw_text_parser_end_element(GMarkupParseContext *context, const gchar *element_name, gpointer user_data, GError **error)
{
    GString *string = user_data;

    if ( g_strcmp0(element_name, "markup") == 0 )
        return;
    if ( g_strcmp0(element_name, "a") == 0 )
        return;

    g_string_append_c(g_string_append(g_string_append_c(g_string_append_c(string, '<'), '/'), element_name), '>');
}

static void
_eventd_nd_draw_text_parser_text(GMarkupParseContext *context, const gchar *text, gsize len, gpointer user_data, GError **error)
{
    GString *string = user_data;

    const gchar *w, *e;

    for ( w = text, e = text + len ; w < e ; w = g_utf8_next_char(w) )
    {
        gunichar c = g_utf8_get_char(w);
        switch ( c )
        {
        case '&':
            g_string_append(string, "&amp;");
        break;
        case '<':
            g_string_append(string, "&lt;");
        break;
        default:
            g_string_append_unichar(string, c);
        break;
        }
    }
}

static GMarkupParser _eventd_nd_draw_text_parser = {
    .start_element = _eventd_nd_draw_text_parser_start_element,
    .end_element = _eventd_nd_draw_text_parser_end_element,
    .text = _eventd_nd_draw_text_parser_text,
    .passthrough = _eventd_nd_draw_text_parser_text,
};

static gchar *
_eventd_nd_draw_parse_text(gchar *text)
{
    GString *string;
    gsize size;
    GError *error = NULL;
    GMarkupParseContext *context;

    size = strlen(text);
    string = g_string_sized_new(size);

    context = g_markup_parse_context_new(&_eventd_nd_draw_text_parser, 0, string, NULL);
    if ( ! g_markup_parse_context_parse(context, "<markup>", strlen("<markup>"), &error) )
        g_warning("Could not parse <markup>: %s", error->message);
    else if ( ! g_markup_parse_context_parse(context, text, size, &error) )
        g_warning("Could not parse escaped text: %s", error->message);
    else if ( ! g_markup_parse_context_parse(context, "</markup>", strlen("</markup>"), &error) )
        g_warning("Could not parse </markup>: %s", error->message);
    else if ( ! g_markup_parse_context_end_parse(context, &error) )
        g_warning("Could not end parsing escaped body: %s", error->message);
    g_markup_parse_context_free(context);
    g_free(text);

    if ( error == NULL )
        return g_string_free(string, FALSE);

    g_clear_error(&error);
    g_string_free(string, TRUE);
    return NULL;
}

static gchar *
_eventd_nd_draw_get_text(EventdNdStyle *style, EventdEvent *event, guint more_size, guint8 *max_lines)
{
    /*
     * This function depends on the current Pango implementation,
     * which is limiting on a per-paragraph basis.
     * If they switch to a per-layout basis, this whole function will be
     * replaced by a simple pango_layout_set_height(-lines) call.
     */

    gchar *text;
    if ( event == NULL)
        text = g_strdup_printf("+%u", more_size);
    else
        text = evhelpers_format_string_get_string(eventd_nd_style_get_template_text(style), event, NULL, NULL);
    if ( *text == '\0' )
    {
        g_free(text);
        return NULL;
    }

    text = _eventd_nd_draw_parse_text(text);
    if ( text == NULL )
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
eventd_nd_draw_text_process(EventdNdStyle *style, EventdEvent *event, gint max_width, guint more_size, gint *text_width)
{
    gchar *text_;
    guint8 max_lines = 0;

    text_ = _eventd_nd_draw_get_text(style, event, more_size, &max_lines);
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
    pango_layout_set_ellipsize(text, eventd_nd_style_get_text_ellipsize(style));
    pango_layout_set_width(text, max_width * PANGO_SCALE);
    if ( max_lines < 1 )
        pango_layout_set_height(text, -max_lines);
    pango_layout_set_markup(text, text_, -1);
    pango_layout_get_pixel_size(text, text_width, NULL);
    g_free(text_);

    g_object_unref(pango_context);

    return text;
}

/*
 * _eventd_nd_draw_get_icon_surface and alpha_mult
 * are inspired by gdk_cairo_set_source_pixbuf
 * GDK is:
 *     Copyright (C) 2011-2018 Red Hat, Inc.
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
    cairo_surface_flush(surface);

    return surface;
}

static cairo_surface_t *
_eventd_nd_draw_limit_size(GdkPixbuf *pixbuf, EventdNdStyle *style, gboolean image, gint max_draw_width)
{
    cairo_surface_t *source;
    gint width, height;
    gint max_width, max_height;
    gboolean fixed_size;

    source = _eventd_nd_draw_get_surface_from_pixbuf(pixbuf);

    width = cairo_image_surface_get_width(source);
    height = cairo_image_surface_get_height(source);

    if ( image )
    {
        eventd_nd_style_get_image_size(style, max_draw_width, &max_width, &max_height);
        fixed_size = eventd_nd_style_get_image_fixed_size(style);
    }
    else
    {
        eventd_nd_style_get_icon_size(style, max_draw_width, &max_width, &max_height);
        fixed_size = eventd_nd_style_get_icon_fixed_size(style);
    }

    if ( ( width < max_width ) && ( height < max_height ) && ( ! fixed_size ) )
        return source;

    gdouble hs = 1.0, vs = 1.0, s;
    if ( ( width > max_width ) || fixed_size )
        hs = (gdouble) max_width / (gdouble) width;
    if ( ( height > max_height ) || fixed_size )
        vs = (gdouble) max_height / (gdouble) height;

    s = MIN(hs, vs);

    width *= s;
    height *= s;

    cairo_surface_t *surface;
    cairo_t *cr;
    cairo_pattern_t *pattern;
    cairo_matrix_t matrix;

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

    cairo_surface_flush(surface);
    return surface;
}

static cairo_surface_t *
_eventd_nd_draw_image_process(GdkPixbuf *pixbuf, EventdNdStyle *style, gint max_draw_width, gint *width, gint *height)
{
    cairo_surface_t *image;

    image = _eventd_nd_draw_limit_size(pixbuf, style, TRUE, max_draw_width);

    *width = cairo_image_surface_get_width(image) + eventd_nd_style_get_image_margin(style);
    *height = cairo_image_surface_get_height(image);

    return image;
}

static cairo_surface_t *
_eventd_nd_draw_icon_process_overlay(GdkPixbuf *pixbuf, EventdNdStyle *style, gint max_draw_width, gint *width, gint *height)
{
    cairo_surface_t *icon;
    gint w, h;

    icon = _eventd_nd_draw_limit_size(pixbuf, style, FALSE, max_draw_width);

    w = cairo_image_surface_get_width(icon);
    h = cairo_image_surface_get_height(icon);

    *width += ( w / 4 );
    *height += ( h / 4 );

    return icon;
}

static cairo_surface_t *
_eventd_nd_draw_icon_process_foreground(GdkPixbuf *pixbuf, EventdNdStyle *style, gint max_draw_width, gint *width, gint *height)
{
    cairo_surface_t *icon;

    icon = _eventd_nd_draw_limit_size(pixbuf, style, FALSE, max_draw_width);

    gint h;

    *width += cairo_image_surface_get_width(icon) + eventd_nd_style_get_icon_margin(style);
    h = cairo_image_surface_get_height(icon);
    *height = MAX(*height, h);

    return icon;
}

static cairo_surface_t *
_eventd_nd_draw_icon_process_background(GdkPixbuf *pixbuf, EventdNdStyle *style, gint max_width, gint *width, gint *height)
{
    cairo_surface_t *icon;

    icon = _eventd_nd_draw_icon_process_foreground(pixbuf, style, max_width, width, height);

    *width -= cairo_image_surface_get_width(icon) * eventd_nd_style_get_icon_fade_width(style) / 4;

    return icon;
}

void
eventd_nd_draw_image_and_icon_process(NkXdgThemeContext *theme_context, EventdNdStyle *style, EventdEvent *event, gint max_width, gint scale, cairo_surface_t **image, cairo_surface_t **icon, gint *text_x, gint *width, gint *height)
{
    gint load_width, load_height;
    GdkPixbuf *image_pixbuf = NULL;
    GdkPixbuf *icon_pixbuf = NULL;
    const Filename *image_filename = eventd_nd_style_get_template_image(style);
    const gchar *image_theme = eventd_nd_style_get_image_theme(style);
    const Filename *icon_filename = eventd_nd_style_get_template_icon(style);
    const gchar *icon_theme = eventd_nd_style_get_icon_theme(style);
    gchar *uri;
    GVariant *data;

    eventd_nd_style_get_image_size(style, max_width, &load_width, &load_height);
    switch ( evhelpers_filename_process(image_filename, event, "images", &uri, &data) )
    {
    case FILENAME_PROCESS_RESULT_URI:
        image_pixbuf = eventd_nd_pixbuf_from_uri(uri, load_width, load_height, scale);
    break;
    case FILENAME_PROCESS_RESULT_DATA:
        image_pixbuf = eventd_nd_pixbuf_from_data(data, load_width, load_height, scale);
    break;
    case FILENAME_PROCESS_RESULT_THEME:
        image_pixbuf = eventd_nd_pixbuf_from_theme(theme_context, image_theme, uri, MIN(load_width, load_height), scale);
    break;
    case FILENAME_PROCESS_RESULT_NONE:
    break;
    }

    eventd_nd_style_get_icon_size(style, max_width, &load_width, &load_height);
    switch ( evhelpers_filename_process(icon_filename, event, "icons", &uri, &data) )
    {
    case FILENAME_PROCESS_RESULT_URI:
        icon_pixbuf = eventd_nd_pixbuf_from_uri(uri, load_width, load_height, scale);
    break;
    case FILENAME_PROCESS_RESULT_DATA:
        icon_pixbuf = eventd_nd_pixbuf_from_data(data, load_width, load_height, scale);
    break;
    case FILENAME_PROCESS_RESULT_THEME:
        icon_pixbuf = eventd_nd_pixbuf_from_theme(theme_context, icon_theme, uri, MIN(load_width, load_height), scale);
    break;
    case FILENAME_PROCESS_RESULT_NONE:
    break;
    }

    *text_x = 0;
    *width = 0;
    *height = 0;
    switch ( eventd_nd_style_get_icon_placement(style) )
    {
    case EVENTD_ND_STYLE_ICON_PLACEMENT_BACKGROUND:
        if ( image_pixbuf != NULL )
        {
            *image = _eventd_nd_draw_image_process(image_pixbuf, style, max_width, width, height);
            *text_x = *width;
            max_width -= *width;
        }
        if ( ( icon_pixbuf != NULL ) && ( max_width > 0 ) )
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
            *image = _eventd_nd_draw_image_process(image_pixbuf, style, max_width, width, height);
            max_width -= *width;
            if ( ( icon_pixbuf != NULL ) && ( max_width > 0 ) )
                *icon = _eventd_nd_draw_icon_process_overlay(icon_pixbuf, style, max_width, width, height);
            *text_x = *width;
        }
    break;
    case EVENTD_ND_STYLE_ICON_PLACEMENT_FOREGROUND:
        if ( image_pixbuf != NULL )
        {
            *image = _eventd_nd_draw_image_process(image_pixbuf, style, max_width, width, height);
            *text_x = *width;
            max_width -= *width;
        }
        if ( ( icon_pixbuf != NULL ) && ( max_width > 0 ) )
            *icon = _eventd_nd_draw_icon_process_foreground(icon_pixbuf, style, max_width, width, height);
    break;
    }
    if ( image_pixbuf != NULL )
        g_object_unref(image_pixbuf);
    if ( icon_pixbuf != NULL )
        g_object_unref(icon_pixbuf);
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

    if ( radius == 0 )
    {
        cairo_rectangle(cr, 0, 0, width, height);
        return;
    }

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
eventd_nd_draw_bubble_draw(cairo_t *cr, EventdNdStyle *style, gint width, gint height, EventdNdShaping shaping, gdouble value)
{
    gint border, blur = 0, radius = 0;
    Colour colour;

    border = eventd_nd_style_get_bubble_border(style);
    cairo_translate(cr, border, border);

    switch ( shaping )
    {
    case EVENTD_ND_SHAPING_NONE:
    break;
    case EVENTD_ND_SHAPING_COMPOSITING:
        blur = eventd_nd_style_get_bubble_border_blur(style);
        cairo_translate(cr, blur * 2, blur * 2);
        /* fallthrough */
    case EVENTD_ND_SHAPING_SHAPE:
        radius = eventd_nd_style_get_bubble_radius(style);
    }

    _eventd_nd_draw_bubble_shape(cr, radius, width, height);
    colour = eventd_nd_style_get_bubble_border_colour(style);
    cairo_set_source_rgba(cr, colour.r, colour.g, colour.b, colour.a);

    if ( blur > 0 )
    {
        cairo_fill_preserve(cr);
        eventd_nd_draw_blur_surface(cr, blur);
    }
    else if ( border > 0 )
    {
        cairo_set_line_width(cr, border * 2);
        cairo_stroke_preserve(cr);
    }

    colour = eventd_nd_style_get_bubble_colour(style);
    cairo_set_source_rgba(cr, colour.r, colour.g, colour.b, colour.a);
    cairo_fill(cr);

    if ( value < 0 )
        return;

    switch ( eventd_nd_style_get_progress_placement(style) )
    {
    case EVENTD_ND_STYLE_PROGRESS_PLACEMENT_BAR_BOTTOM:
    break;
    case EVENTD_ND_STYLE_PROGRESS_PLACEMENT_IMAGE_BOTTOM_TOP:
    case EVENTD_ND_STYLE_PROGRESS_PLACEMENT_IMAGE_TOP_BOTTOM:
    case EVENTD_ND_STYLE_PROGRESS_PLACEMENT_IMAGE_LEFT_RIGHT:
    case EVENTD_ND_STYLE_PROGRESS_PLACEMENT_IMAGE_RIGHT_LEFT:
    case EVENTD_ND_STYLE_PROGRESS_PLACEMENT_IMAGE_CIRCULAR:
        return;
    }


    gint bar_width;
    cairo_pattern_t *mask;
    gdouble stop = 0.0;

    bar_width = eventd_nd_style_get_progress_bar_width(style);
    cairo_save(cr);
    _eventd_nd_draw_bubble_shape(cr, radius, width, height);
    cairo_clip(cr);
    cairo_rectangle(cr, 0, height - bar_width, width, bar_width);
    cairo_clip(cr);

    if ( value < 0.001 )
        stop = 0;
    else if ( value > 0.999 )
        stop = 1.0;
    else
        stop = (gdouble) ( (gdouble) ( width - 2 * radius ) * value + radius ) / width;

    mask = cairo_pattern_create_linear(0, 0, width, 0);
    cairo_pattern_add_color_stop_rgba(mask, stop, 0, 0, 0, 1);
    cairo_pattern_add_color_stop_rgba(mask, stop, 0, 0, 0, 0.5);

    colour = eventd_nd_style_get_progress_colour(style);
    cairo_set_source_rgba(cr, colour.r, colour.g, colour.b, colour.a);
    cairo_mask(cr, mask);
    cairo_pattern_destroy(mask);

    cairo_restore(cr);
}

void
eventd_nd_draw_text_draw(cairo_t *cr, EventdNdStyle *style, PangoLayout *text, gint offset_x, gint offset_y)
{
    Colour colour;

    colour = eventd_nd_style_get_text_colour(style);
    cairo_set_source_rgba(cr, colour.r, colour.g, colour.b, colour.a);
    cairo_new_path(cr);
    cairo_move_to(cr, offset_x, offset_y);
    pango_cairo_update_layout(cr, text);
    pango_cairo_show_layout(cr, text);
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
    g_return_val_if_reached(-1);
}

static void
_eventd_nd_draw_surface_draw(cairo_t *cr, cairo_surface_t *surface, gint x, gint y, cairo_pattern_t *mask)
{
    cairo_set_source_surface(cr, surface, x, y);
    cairo_rectangle(cr, x, y, cairo_image_surface_get_width(surface), cairo_image_surface_get_height(surface));
    if ( mask == NULL )
        cairo_fill(cr);
    else
    {
        cairo_mask(cr, mask);
        cairo_pattern_destroy(mask);
    }
}

static gint
_eventd_nd_draw_image_draw(cairo_t *cr, cairo_surface_t *image, EventdNdStyle *style, gint width, gint height, gdouble value)
{
    gint image_height, image_width;
    gint x, y;
    cairo_pattern_t *mask = NULL;

    image_width = cairo_image_surface_get_width(image);
    image_height = cairo_image_surface_get_height(image);
    x = 0;
    y = _eventd_nd_draw_get_valign(eventd_nd_style_get_image_anchor(style), height, image_height);

    if ( value < 0 )
        goto draw;

    gint x1, x2, y1, y2;
    x1 = x2 = x;
    y1 = y2 = y;

    switch ( eventd_nd_style_get_progress_placement(style) )
    {
    case EVENTD_ND_STYLE_PROGRESS_PLACEMENT_IMAGE_BOTTOM_TOP:
        y2 += image_height;
    break;
    case EVENTD_ND_STYLE_PROGRESS_PLACEMENT_IMAGE_TOP_BOTTOM:
        y1 += image_height;
    break;
    case EVENTD_ND_STYLE_PROGRESS_PLACEMENT_IMAGE_LEFT_RIGHT:
        x2 += image_width;
    break;
    case EVENTD_ND_STYLE_PROGRESS_PLACEMENT_IMAGE_RIGHT_LEFT:
        x1 += image_width;
    break;
    case EVENTD_ND_STYLE_PROGRESS_PLACEMENT_IMAGE_CIRCULAR:
    {
        cairo_save(cr);
        cairo_push_group_with_content(cr, CAIRO_CONTENT_ALPHA);

        gdouble xc = x + image_width / 2;
        gdouble yc = y + image_height / 2;
        gdouble r = G_SQRT2 * MAX(image_width, image_height);
        gdouble a = ( 2 * value - 0.5 ) * G_PI;

        cairo_set_source_rgba(cr, 0, 0, 0, 0.5);
        cairo_paint(cr);

        cairo_set_source_rgba(cr, 0, 0, 0, 1.0);
        cairo_move_to(cr, xc, yc);
        cairo_arc(cr, xc, yc, r, - G_PI_2, a);
        cairo_close_path(cr);
        cairo_fill(cr);

        cairo_pop_group_to_source(cr);
        mask = cairo_pattern_reference(cairo_get_source(cr));
        cairo_restore(cr);

        goto draw;
    }
    case EVENTD_ND_STYLE_PROGRESS_PLACEMENT_BAR_BOTTOM:
        goto draw;
    }

    mask = cairo_pattern_create_linear(x1, y1, x2, y2);
    cairo_pattern_add_color_stop_rgba(mask, value, 0, 0, 0, 1);
    cairo_pattern_add_color_stop_rgba(mask, value, 0, 0, 0, 0.5);

draw:
    _eventd_nd_draw_surface_draw(cr, image, x, y, mask);

    return y;
}

static void
_eventd_nd_draw_image_and_icon_draw_overlay(cairo_t *cr, cairo_surface_t *image, cairo_surface_t *icon, EventdNdStyle *style, gint width, gint height, gdouble value)
{
    if ( image == NULL )
        return;

    gint image_y;
    image_y = _eventd_nd_draw_image_draw(cr, image, style, width, height, value);

    if ( icon == NULL )
        return;

    gint icon_x, icon_y;
    gint w, h;

    w = cairo_image_surface_get_width(icon);
    h =  cairo_image_surface_get_height(icon);

    icon_x = cairo_image_surface_get_width(image) - ( 3 * w / 4 );
    icon_y = image_y + cairo_image_surface_get_height(image) - ( 3 * h / 4 );

    _eventd_nd_draw_surface_draw(cr, icon, icon_x, icon_y, NULL);
}

static void
_eventd_nd_draw_image_and_icon_draw_foreground(cairo_t *cr, cairo_surface_t *image, cairo_surface_t *icon, EventdNdStyle *style, gint width, gint height, gdouble value)
{
    if ( image != NULL )
        _eventd_nd_draw_image_draw(cr, image, style, width, height, value);

    if ( icon == NULL )
        return;

    gint x, y;

    x = width - cairo_image_surface_get_width(icon);
    y = _eventd_nd_draw_get_valign(eventd_nd_style_get_icon_anchor(style), height, cairo_image_surface_get_height(icon));
    _eventd_nd_draw_surface_draw(cr, icon, x, y, NULL);
}

static void
_eventd_nd_draw_image_and_icon_draw_background(cairo_t *cr, cairo_surface_t *image, cairo_surface_t *icon, EventdNdStyle *style, gint width, gint height, gdouble value)
{
    if ( image != NULL )
        _eventd_nd_draw_image_draw(cr, image, style, width, height, value);

    if ( icon == NULL )
        return;

    gint x1, x2, y;
    cairo_pattern_t *mask;

    x2 = width;
    x1 = x2 - cairo_image_surface_get_width(icon);
    y = _eventd_nd_draw_get_valign(eventd_nd_style_get_icon_anchor(style), height, cairo_image_surface_get_height(icon));

    mask = cairo_pattern_create_linear(x1, 0, x2, 0);
    cairo_pattern_add_color_stop_rgba(mask, 0, 0, 0, 0, 0);
    cairo_pattern_add_color_stop_rgba(mask, eventd_nd_style_get_icon_fade_width(style), 0, 0, 0, 1);

    _eventd_nd_draw_surface_draw(cr, icon, x1, y, mask);
}

void
eventd_nd_draw_image_and_icon_draw(cairo_t *cr, cairo_surface_t *image, cairo_surface_t *icon, EventdNdStyle *style, gint width, gint height, gdouble value)
{
    switch ( eventd_nd_style_get_icon_placement(style) )
    {
    case EVENTD_ND_STYLE_ICON_PLACEMENT_BACKGROUND:
        _eventd_nd_draw_image_and_icon_draw_background(cr, image, icon, style, width, height, value);
    break;
    case EVENTD_ND_STYLE_ICON_PLACEMENT_OVERLAY:
        _eventd_nd_draw_image_and_icon_draw_overlay(cr, image, icon, style, width, height, value);
    break;
    case EVENTD_ND_STYLE_ICON_PLACEMENT_FOREGROUND:
        _eventd_nd_draw_image_and_icon_draw_foreground(cr, image, icon, style, width, height, value);
    break;
    }
}
