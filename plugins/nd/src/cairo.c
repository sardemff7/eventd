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

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#ifdef HAVE_MATH_H
#include <math.h>
#endif /* HAVE_MATH_H */

#include <glib.h>
#include <glib-object.h>

#include <cairo.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>

#include <libeventd-event.h>
#include <libeventd-helpers-config.h>

#include "style.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "icon.h"

#include "cairo.h"

struct _EventdNdBubble {
    GList *surfaces;
};


static gssize
_eventd_nd_cairo_strccount(const gchar *str, char c)
{
    gssize count = 1;
    if ( str == NULL )
        return -1;
    if ( *str == '\0' )
        return 0;
    for ( ; *str != 0 ; ++str )
    {
        if ( *str == c )
            ++count;
    }
    return count;
}

static gchar *
_eventd_nd_cairo_get_message(gchar *message, guint8 max)
{
    gssize count;
    gchar **message_lines;

    if ( ( max < 1 ) || ( ( count = _eventd_nd_cairo_strccount(message, '\n') ) <= max ) )
        return message;

    message_lines = g_strsplit(message, "\n", -1);

    GString *message_str;
    guint i;

    message_str = g_string_sized_new(strlen(message));

    for ( i = 0 ; i < ( max / 2 ) ; ++i )
        message_str = g_string_append_c(g_string_append(message_str, message_lines[i]), '\n');
    /* Ellipsize */
    g_string_append(message_str, "[…]");
    for ( i = count - ( ( max + 1 ) / 2 ) ; i < count ; ++i )
        message_str = g_string_append(g_string_append_c(message_str, '\n'), message_lines[i]);
    g_strfreev(message_lines);


    g_free(message);
    return g_string_free(message_str, FALSE);
}

void
eventd_nd_cairo_text_process(EventdNdStyle *style, EventdEvent *event, gint max_width, PangoLayout **title, PangoLayout **message, gint *text_height, gint *text_width)
{
    PangoContext *pango_context;
    gchar *text;

    pango_context = pango_context_new();
    pango_context_set_font_map(pango_context, pango_cairo_font_map_get_default());

    text = evhelpers_format_string_get_string(eventd_nd_style_get_template_title(style), event, NULL, NULL);
    *title = pango_layout_new(pango_context);
    pango_layout_set_font_description(*title, eventd_nd_style_get_title_font(style));
    pango_layout_set_alignment(*title, eventd_nd_style_get_title_align(style));
    pango_layout_set_ellipsize(*title, PANGO_ELLIPSIZE_MIDDLE);
    if ( max_width > -1 )
        pango_layout_set_width(*title, max_width * PANGO_SCALE);
    pango_layout_set_text(*title, text, -1);
    pango_layout_get_pixel_size(*title, text_width, text_height);
    g_free(text);

    text = evhelpers_format_string_get_string(eventd_nd_style_get_template_message(style), event, NULL, NULL);
    if ( ( text != NULL ) && ( *text != '\0' ) )
    {
        gint w;
        gint h;

        text = _eventd_nd_cairo_get_message(text, eventd_nd_style_get_message_max_lines(style));

        *message = pango_layout_new(pango_context);
        pango_layout_set_font_description(*message, eventd_nd_style_get_message_font(style));
        pango_layout_set_alignment(*message, eventd_nd_style_get_message_align(style));
        pango_layout_set_ellipsize(*message, PANGO_ELLIPSIZE_MIDDLE);
        pango_layout_set_height(*message, -eventd_nd_style_get_message_max_lines(style));
        if ( max_width > -1 )
            pango_layout_set_width(*message, max_width * PANGO_SCALE);
        pango_layout_set_markup(*message, text, -1);
        pango_layout_get_pixel_size(*message, &w, &h);


        *text_height += eventd_nd_style_get_message_spacing(style) + h;
        *text_width = MAX(*text_width, w);
    }
    g_free(text);

    g_object_unref(pango_context);
}

void
eventd_nd_cairo_bubble_draw(cairo_t *cr, Colour colour, gint radius, gint width, gint height)
{
    cairo_set_source_rgba(cr, colour.r, colour.g, colour.b, colour.a);

    if ( radius < 1 )
    {
        cairo_paint(cr);
        return;
    }

    gint limit;

    limit = MIN(width, height) / 2;

    if ( radius > limit )
        radius = limit;

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

void
eventd_nd_cairo_text_draw(cairo_t *cr, EventdNdStyle *style, PangoLayout *title, PangoLayout *message, gint offset_x, gint offset_y, gint max_height)
{
    gint title_height;
    pango_layout_get_pixel_size(title, NULL, &title_height);

    if ( message == NULL )
        offset_y += ( max_height - title_height ) / 2;

    Colour colour;

    colour = eventd_nd_style_get_title_colour(style);
    cairo_set_source_rgba(cr, colour.r, colour.g, colour.b, colour.a);
    cairo_new_path(cr);
    cairo_move_to(cr, offset_x, offset_y);
    pango_cairo_update_layout(cr, title);
    pango_cairo_layout_path(cr, title);
    cairo_fill(cr);

    if ( message != NULL )
    {
        offset_y += eventd_nd_style_get_message_spacing(style) + title_height;

        colour = eventd_nd_style_get_message_colour(style);
        cairo_set_source_rgba(cr, colour.r, colour.g, colour.b, colour.a);
        cairo_new_path(cr);
        cairo_move_to(cr, offset_x, offset_y);
        pango_cairo_update_layout(cr, message);
        pango_cairo_layout_path(cr, message);
        cairo_fill(cr);
    }
}
