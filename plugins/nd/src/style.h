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

#ifndef __EVENTD_ND_STYLE_H__
#define __EVENTD_ND_STYLE_H__

typedef struct _EventdNdStyle EventdNdStyle;
typedef struct _EventdNdNotificationContents EventdNdNotificationContents;

typedef enum {
    EVENTD_ND_ANCHOR_TOP     = 1<<0,
    EVENTD_ND_ANCHOR_BOTTOM  = 1<<1,
    EVENTD_ND_ANCHOR_VCENTER = 3<<0,
    EVENTD_ND_ANCHOR_LEFT    = 1<<2,
    EVENTD_ND_ANCHOR_RIGHT   = 1<<3,
    EVENTD_ND_ANCHOR_HCENTER = 3<<2
} EventdNdAnchor;

typedef enum {
    EVENTD_ND_STYLE_ICON_PLACEMENT_BACKGROUND,
    EVENTD_ND_STYLE_ICON_PLACEMENT_OVERLAY,
    EVENTD_ND_STYLE_ICON_PLACEMENT_FOREGROUND
} EventdNdStyleIconPlacement;

EventdNdStyle *eventd_nd_style_new(EventdNdStyle *parent);
void eventd_nd_style_free(gpointer style);

void eventd_nd_style_update(EventdNdStyle *style, GKeyFile *config_file, gint *max_width, gint *max_height);

const gchar *eventd_nd_style_get_template_title(EventdNdStyle *style);
const gchar *eventd_nd_style_get_template_message(EventdNdStyle *style);
const gchar *eventd_nd_style_get_template_image(EventdNdStyle *style);
const gchar *eventd_nd_style_get_template_icon(EventdNdStyle *style);

gint eventd_nd_style_get_bubble_min_width(EventdNdStyle *style);
gint eventd_nd_style_get_bubble_max_width(EventdNdStyle *style);

gint eventd_nd_style_get_bubble_padding(EventdNdStyle *style);
gint eventd_nd_style_get_bubble_radius(EventdNdStyle *style);
Colour eventd_nd_style_get_bubble_colour(EventdNdStyle *style);

const PangoFontDescription *eventd_nd_style_get_title_font(EventdNdStyle *style);
Colour eventd_nd_style_get_title_colour(EventdNdStyle *style);

gint eventd_nd_style_get_message_spacing(EventdNdStyle *style);
guint8 eventd_nd_style_get_message_max_lines(EventdNdStyle *style);
const PangoFontDescription *eventd_nd_style_get_message_font(EventdNdStyle *style);
Colour eventd_nd_style_get_message_colour(EventdNdStyle *style);

gint eventd_nd_style_get_image_max_width(EventdNdStyle *style);
gint eventd_nd_style_get_image_max_height(EventdNdStyle *style);
gint eventd_nd_style_get_image_margin(EventdNdStyle *style);

EventdNdStyleIconPlacement eventd_nd_style_get_icon_placement(EventdNdStyle *style);
EventdNdAnchor eventd_nd_style_get_icon_anchor(EventdNdStyle *style);
gint eventd_nd_style_get_icon_max_width(EventdNdStyle *style);
gint eventd_nd_style_get_icon_max_height(EventdNdStyle *style);
gint eventd_nd_style_get_icon_margin(EventdNdStyle *style);
gdouble eventd_nd_style_get_icon_fade_width(EventdNdStyle *style);


EventdNdNotificationContents *eventd_nd_notification_contents_new(EventdNdStyle *style, EventdEvent *event, gint width, gint height);
void eventd_nd_notification_contents_free(EventdNdNotificationContents *notification);

const gchar *eventd_nd_notification_contents_get_title(EventdNdNotificationContents *notification);
const gchar *eventd_nd_notification_contents_get_message(EventdNdNotificationContents *notification);

#endif /* __EVENTD_ND_STYLE_H__ */
