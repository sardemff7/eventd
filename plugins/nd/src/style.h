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

#ifndef __EVENTD_ND_STYLE_H__
#define __EVENTD_ND_STYLE_H__

typedef struct _EventdPluginAction EventdNdStyle;

typedef enum {
    EVENTD_ND_ANCHOR_TOP_LEFT,
    EVENTD_ND_ANCHOR_TOP,
    EVENTD_ND_ANCHOR_TOP_RIGHT,
    EVENTD_ND_ANCHOR_BOTTOM_LEFT,
    EVENTD_ND_ANCHOR_BOTTOM,
    EVENTD_ND_ANCHOR_BOTTOM_RIGHT,
    _EVENTD_ND_ANCHOR_SIZE
} EventdNdAnchor;
const gchar * const eventd_nd_anchors[_EVENTD_ND_ANCHOR_SIZE];

typedef enum {
    EVENTD_ND_VANCHOR_TOP,
    EVENTD_ND_VANCHOR_BOTTOM,
    EVENTD_ND_VANCHOR_CENTER,
} EventdNdAnchorVertical;

typedef enum {
    EVENTD_ND_HANCHOR_LEFT,
    EVENTD_ND_HANCHOR_RIGHT,
    EVENTD_ND_HANCHOR_CENTER,
} EventdNdAnchorHorizontal;

typedef enum {
    EVENTD_ND_STYLE_ICON_PLACEMENT_BACKGROUND,
    EVENTD_ND_STYLE_ICON_PLACEMENT_OVERLAY,
    EVENTD_ND_STYLE_ICON_PLACEMENT_FOREGROUND
} EventdNdStyleIconPlacement;

EventdNdStyle *eventd_nd_style_new(EventdNdStyle *parent);
void eventd_nd_style_free(gpointer style);

void eventd_nd_style_update(EventdNdStyle *style, GKeyFile *config_file, gint *max_width, gint *max_height);

FormatString *eventd_nd_style_get_template_text(EventdNdStyle *style);
Filename *eventd_nd_style_get_template_image(EventdNdStyle *style);
Filename *eventd_nd_style_get_template_icon(EventdNdStyle *style);

EventdNdAnchor eventd_nd_style_get_bubble_anchor(EventdNdStyle *style);
gint eventd_nd_style_get_bubble_timeout(EventdNdStyle *style);

gint eventd_nd_style_get_bubble_min_width(EventdNdStyle *style);
gint eventd_nd_style_get_bubble_max_width(EventdNdStyle *style);

gint eventd_nd_style_get_bubble_padding(EventdNdStyle *style);
gint eventd_nd_style_get_bubble_radius(EventdNdStyle *style);
Colour eventd_nd_style_get_bubble_colour(EventdNdStyle *style);

const PangoFontDescription *eventd_nd_style_get_text_font(EventdNdStyle *style);
PangoAlignment eventd_nd_style_get_text_align(EventdNdStyle *style);
guint8 eventd_nd_style_get_text_max_lines(EventdNdStyle *style);
Colour eventd_nd_style_get_text_colour(EventdNdStyle *style);

EventdNdAnchorVertical eventd_nd_style_get_image_anchor(EventdNdStyle *style);
gint eventd_nd_style_get_image_max_width(EventdNdStyle *style);
gint eventd_nd_style_get_image_max_height(EventdNdStyle *style);
gint eventd_nd_style_get_image_margin(EventdNdStyle *style);

EventdNdStyleIconPlacement eventd_nd_style_get_icon_placement(EventdNdStyle *style);
EventdNdAnchorVertical eventd_nd_style_get_icon_anchor(EventdNdStyle *style);
gint eventd_nd_style_get_icon_max_width(EventdNdStyle *style);
gint eventd_nd_style_get_icon_max_height(EventdNdStyle *style);
gint eventd_nd_style_get_icon_margin(EventdNdStyle *style);
gdouble eventd_nd_style_get_icon_fade_width(EventdNdStyle *style);

#endif /* __EVENTD_ND_STYLE_H__ */
