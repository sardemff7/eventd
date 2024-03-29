/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2024 Morgane "Sardem FF7" Glidic
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

#include "types.h"

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

typedef enum {
    EVENTD_ND_STYLE_PROGRESS_PLACEMENT_BAR_BOTTOM,
    EVENTD_ND_STYLE_PROGRESS_PLACEMENT_IMAGE_BOTTOM_TOP,
    EVENTD_ND_STYLE_PROGRESS_PLACEMENT_IMAGE_TOP_BOTTOM,
    EVENTD_ND_STYLE_PROGRESS_PLACEMENT_IMAGE_LEFT_RIGHT,
    EVENTD_ND_STYLE_PROGRESS_PLACEMENT_IMAGE_RIGHT_LEFT,
    EVENTD_ND_STYLE_PROGRESS_PLACEMENT_IMAGE_CIRCULAR,
} EventdNdStyleProgressPlacement;

EventdNdStyle *eventd_nd_style_new(EventdNdStyle *parent);
void eventd_nd_style_free(gpointer style);

void eventd_nd_style_update(EventdNdStyle *style, GKeyFile *config_file);

FormatString *eventd_nd_style_get_template_text(EventdNdStyle *style);
Filename *eventd_nd_style_get_template_image(EventdNdStyle *style);
Filename *eventd_nd_style_get_template_icon(EventdNdStyle *style);
const gchar *eventd_nd_style_get_template_progress(EventdNdStyle *style);

const gchar *eventd_nd_style_get_bubble_queue(EventdNdStyle *style);
gint eventd_nd_style_get_bubble_timeout(EventdNdStyle *style);

gint eventd_nd_style_get_bubble_min_width(EventdNdStyle *style);
gint eventd_nd_style_get_bubble_max_width(EventdNdStyle *style);

gint eventd_nd_style_get_bubble_padding(EventdNdStyle *style);
gint eventd_nd_style_get_bubble_radius(EventdNdStyle *style);
Colour eventd_nd_style_get_bubble_colour(EventdNdStyle *style);
gint eventd_nd_style_get_bubble_border(EventdNdStyle *style);
Colour eventd_nd_style_get_bubble_border_colour(EventdNdStyle *style);
guint64 eventd_nd_style_get_bubble_border_blur(EventdNdStyle *style);
gint64 eventd_nd_style_get_bubble_border_blur_offset_x(EventdNdStyle *style);
gint64 eventd_nd_style_get_bubble_border_blur_offset_y(EventdNdStyle *style);

const PangoFontDescription *eventd_nd_style_get_text_font(EventdNdStyle *style);
PangoAlignment eventd_nd_style_get_text_align(EventdNdStyle *style);
EventdNdAnchorVertical eventd_nd_style_get_text_valign(EventdNdStyle *style);
PangoEllipsizeMode eventd_nd_style_get_text_ellipsize(EventdNdStyle *style);
guint8 eventd_nd_style_get_text_max_lines(EventdNdStyle *style);
gint eventd_nd_style_get_text_max_width(EventdNdStyle *style);
Colour eventd_nd_style_get_text_colour(EventdNdStyle *style);

EventdNdAnchorVertical eventd_nd_style_get_image_anchor(EventdNdStyle *style);
gint eventd_nd_style_get_image_width(EventdNdStyle *style);
gint eventd_nd_style_get_image_height(EventdNdStyle *style);
void eventd_nd_style_get_image_size(EventdNdStyle *style, gint max_draw_width, gint *width, gint *height);
gboolean eventd_nd_style_get_image_fixed_size(EventdNdStyle *style);
gint eventd_nd_style_get_image_margin(EventdNdStyle *style);
const gchar *eventd_nd_style_get_image_theme(EventdNdStyle *style);

EventdNdStyleIconPlacement eventd_nd_style_get_icon_placement(EventdNdStyle *style);
EventdNdAnchorVertical eventd_nd_style_get_icon_anchor(EventdNdStyle *style);
gint eventd_nd_style_get_icon_width(EventdNdStyle *style);
gint eventd_nd_style_get_icon_height(EventdNdStyle *style);
void eventd_nd_style_get_icon_size(EventdNdStyle *style, gint max_draw_width, gint *width, gint *height);
gboolean eventd_nd_style_get_icon_fixed_size(EventdNdStyle *style);
gint eventd_nd_style_get_icon_margin(EventdNdStyle *style);
gdouble eventd_nd_style_get_icon_fade_width(EventdNdStyle *style);
const gchar *eventd_nd_style_get_icon_theme(EventdNdStyle *style);

EventdNdStyleProgressPlacement eventd_nd_style_get_progress_placement(EventdNdStyle *style);
gboolean eventd_nd_style_get_progress_reversed(EventdNdStyle *style);
gint eventd_nd_style_get_progress_bar_width(EventdNdStyle *style);
Colour eventd_nd_style_get_progress_colour(EventdNdStyle *self);

#endif /* __EVENTD_ND_STYLE_H__ */
