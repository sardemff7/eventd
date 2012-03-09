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

#ifndef __EVENTD_ND_STYLE_INTERNAL_H__
#define __EVENTD_ND_STYLE_INTERNAL_H__

typedef struct {
    gdouble r;
    gdouble g;
    gdouble b;
    gdouble a;
} Colour;

struct _EventdNdStyle {
    gint bubble_min_width;
    gint bubble_max_width;

    gint bubble_padding;
    gint bubble_radius;
    Colour bubble_colour;

    gint icon_max_width;
    gint icon_max_height;
    gint icon_spacing;
    gdouble overlay_scale;

    PangoContext *pango_context;

    PangoFontDescription *title_font;
    Colour title_colour;
    gchar *title_font_string;

    gint message_spacing;
    guint8 message_max_lines;
    PangoFontDescription *message_font;
    Colour message_colour;
    gchar *message_font_string;
};


#endif /* __EVENTD_ND_STYLE_INTERNAL_H__ */
