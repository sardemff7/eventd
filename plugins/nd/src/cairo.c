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
    for ( ; *str != 0 ; ++str )
    {
        if ( *str == c )
            ++count;
    }
    return count;
}

static gchar *
_eventd_nd_cairo_find_n_c(gchar *s, gsize n, gunichar c)
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
_eventd_nd_cairo_get_text(EventdNdStyle *style, EventdEvent *event, guint8 *max_lines)
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

    if ( ( count = _eventd_nd_cairo_strccount(text, '\n') ) <= max )
    {
        *max_lines = max / count;
        goto ret;
    }

    gchar *b1, *b2;
    gssize el;

    b1 = _eventd_nd_cairo_find_n_c(text, max / 2, '\n');
    b2 = _eventd_nd_cairo_find_n_c(text, ( count - ( max + 1 ) / 2 + 1 ), '\n');
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
eventd_nd_cairo_text_process(EventdNdStyle *style, EventdEvent *event, gint max_width, gint *text_height, gint *text_width)
{
    gchar *text_;
    guint8 max_lines = 0;

    text_ = _eventd_nd_cairo_get_text(style, event, &max_lines);
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
eventd_nd_cairo_text_draw(cairo_t *cr, EventdNdStyle *style, PangoLayout *text, gint offset_x, gint offset_y, gint max_height)
{
    Colour colour;

    colour = eventd_nd_style_get_text_colour(style);
    cairo_set_source_rgba(cr, colour.r, colour.g, colour.b, colour.a);
    cairo_new_path(cr);
    cairo_move_to(cr, offset_x, offset_y);
    pango_cairo_update_layout(cr, text);
    pango_cairo_layout_path(cr, text);
    cairo_fill(cr);
}
