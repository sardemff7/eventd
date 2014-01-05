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

#include <libeventd-event-types.h>
#include <libeventd-config.h>

#include "style.h"
#ifdef ENABLE_GDK_PIXBUF
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "icon.h"
#endif /* ENABLE_GDK_PIXBUF */

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
_eventd_nd_cairo_get_message(const gchar *message, guint8 max)
{
    gssize count;
    gchar **message_lines;

    count = _eventd_nd_cairo_strccount(message, '\n');
    if ( ( g_strstr_len(message, -1, "\n") == NULL ) || ( max < 1 ) || ( count <= max ) )
        return _eventd_nd_cairo_message_escape(message);

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

    gchar *ret;

    ret = _eventd_nd_cairo_message_escape(message_str->str);
    g_string_free(message_str, TRUE);

    return ret;
}

static void
_eventd_nd_cairo_text_process(EventdNdNotificationContents *notification, EventdNdStyle *style, gint max_width, PangoLayout **title, PangoLayout **message, gint *text_height, gint *text_width)
{
    PangoContext *pango_context;

    pango_context = pango_context_new();
    pango_context_set_font_map(pango_context, pango_cairo_font_map_get_default());

    *title = pango_layout_new(pango_context);
    pango_layout_set_font_description(*title, eventd_nd_style_get_title_font(style));
    pango_layout_set_ellipsize(*title, PANGO_ELLIPSIZE_MIDDLE);
    if ( max_width > -1 )
        pango_layout_set_width(*title, max_width * PANGO_SCALE);
    pango_layout_set_text(*title, eventd_nd_notification_contents_get_title(notification), -1);
    pango_layout_get_pixel_size(*title, text_width, text_height);

    const gchar *tmp;
    tmp = eventd_nd_notification_contents_get_message(notification);
    if ( tmp != NULL )
    {
        gchar *text;
        gint w;
        gint h;

        text = _eventd_nd_cairo_get_message(tmp, eventd_nd_style_get_message_max_lines(style));

        *message = pango_layout_new(pango_context);
        pango_layout_set_font_description(*message, eventd_nd_style_get_message_font(style));
        pango_layout_set_ellipsize(*message, PANGO_ELLIPSIZE_MIDDLE);
        pango_layout_set_height(*message, -eventd_nd_style_get_message_max_lines(style));
        if ( max_width > -1 )
            pango_layout_set_width(*message, max_width * PANGO_SCALE);
        pango_layout_set_markup(*message, text, -1);
        pango_layout_get_pixel_size(*message, &w, &h);

        g_free(text);

        *text_height += eventd_nd_style_get_message_spacing(style) + h;
        *text_width = MAX(*text_width, w);
    }

    g_object_unref(pango_context);
}

static void
_eventd_nd_cairo_bubble_draw(cairo_t *cr, Colour colour, gint radius, gint width, gint height)
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

    cairo_set_source_rgba(cr, colour.r, colour.g, colour.b, colour.a);
    cairo_fill(cr);
}

static void
_eventd_nd_cairo_text_draw(cairo_t *cr, EventdNdStyle *style, PangoLayout *title, PangoLayout *message, gint offset_x, gint offset_y, gint max_width, gint max_height)
{
    gint title_height;
    pango_layout_get_pixel_size(title, NULL, &title_height);

    if ( message == NULL )
        offset_y += ( max_height - title_height ) / 2;
    pango_layout_set_width(title, max_width * PANGO_SCALE);

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
        pango_layout_set_width(message, max_width * PANGO_SCALE);
        pango_cairo_update_layout(cr, message);
        pango_cairo_layout_path(cr, message);
        cairo_fill(cr);

        g_object_unref(message);
    }

    g_object_unref(title);
}

cairo_surface_t *
eventd_nd_cairo_get_surface(EventdEvent *event, EventdNdNotificationContents *notification, EventdNdStyle *style)
{
    gint padding;
    gint min_width, max_width;

    gint width;
    gint height;

    PangoLayout *title;
    PangoLayout *message = NULL;

#ifdef ENABLE_GDK_PIXBUF
    cairo_surface_t *image = NULL;
    cairo_surface_t *icon = NULL;
#endif /* ENABLE_GDK_PIXBUF */

    gint content_height;
    gint text_width = 0, text_height = 0;
    gint text_margin = 0;
    gint image_width = 0, image_height = 0;

    cairo_t *cr;

    padding = eventd_nd_style_get_bubble_padding(style);
    min_width = eventd_nd_style_get_bubble_min_width(style);
    max_width = eventd_nd_style_get_bubble_max_width(style);

    /* proccess data and compute the bubble size */
    _eventd_nd_cairo_text_process(notification, style, ( max_width > -1 ) ? ( max_width - 2 * padding ) : -1, &title, &message, &text_height, &text_width);

    width = 2 * padding + text_width;

#ifdef ENABLE_GDK_PIXBUF
    if ( ( max_width < 0 ) || ( width < max_width ) )
        eventd_nd_cairo_image_and_icon_process(notification, style, ( max_width > -1 ) ? ( max_width - width ) : -1, &image, &icon, &text_margin, &image_width, &image_height);
    width += image_width;
#endif /* ENABLE_GDK_PIXBUF */

    /* We are sure that min_width < max_width */
    if ( ( min_width > -1 ) && ( min_width > width ) )
    {
        width = min_width;
        /* Let the text take the remaining space if needed (e.g. Right-to-Left) */
        text_width = width - ( 2 * padding + image_width );
    }

    content_height = MAX(image_height, text_height);
    height = 2 * padding + content_height;

    cairo_surface_t *bubble;

    /* draw the bubble */
    bubble = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cr = cairo_create(bubble);
    _eventd_nd_cairo_bubble_draw(cr, eventd_nd_style_get_bubble_colour(style), eventd_nd_style_get_bubble_radius(style), width, height);
#ifdef ENABLE_GDK_PIXBUF
    eventd_nd_cairo_image_and_icon_draw(cr, image, icon, style, width, height);
#endif /* ENABLE_GDK_PIXBUF */
    _eventd_nd_cairo_text_draw(cr, style, title, message, padding + text_margin, padding, text_width, content_height);
    cairo_destroy(cr);

    return bubble;
}
