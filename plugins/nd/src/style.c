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

#include <glib.h>
#include <pango/pango.h>

#include <libeventd-config.h>
#include <libeventd-nd-notification-template.h>

#include "style.h"

struct _EventdNdStyle {
    LibeventdNdNotificationTemplate *template;

    EventdNdStyle *parent;

    struct {
        gboolean set;

        gint   min_width;
        gint   max_width;

        gint   padding;
        gint   radius;
        Colour colour;
    } bubble;

    struct {
        gboolean set;

        gint max_width;
        gint max_height;
        gint margin;
    } image;

    struct {
        gboolean set;

        EventdNdStyleIconPlacement placement;
        EventdNdAnchor             anchor;
        gint                       max_width;
        gint                       max_height;
        gint                       margin;
        gdouble                    fade_width;
    } icon;

    struct {
        gboolean set;

        PangoFontDescription *font;
        Colour colour;
    } title;

    struct {
        gboolean set;

        gint   spacing;
        guint8 max_lines;
        PangoFontDescription *font;
        Colour colour;
    } message;
};

static void
_eventd_nd_style_init_defaults(EventdNdStyle *style)
{
    style->bubble.set = TRUE;

    /* bubble geometry */
    style->bubble.min_width = 200;
    style->bubble.max_width = -1;

    /* bubble style */
    style->bubble.padding   = 10;
    style->bubble.radius    = 10;
    style->bubble.colour.r = 0.15;
    style->bubble.colour.g = 0.15;
    style->bubble.colour.b = 0.15;
    style->bubble.colour.a = 1.0;

    /* image */
    style->image.set = TRUE;
    style->image.max_width  = 50;
    style->image.max_height = 50;
    style->image.margin     = 10;

    /* icon */
    style->icon.set = TRUE;
    style->icon.placement  = EVENTD_ND_STYLE_ICON_PLACEMENT_BACKGROUND;
    style->icon.anchor     = EVENTD_ND_ANCHOR_VCENTER;
    style->icon.max_width  = 50;
    style->icon.max_height = 50;
    style->icon.margin     = 10;
    style->icon.fade_width = 0.75;

    /* title */
    style->title.set = TRUE;
    style->title.font        = pango_font_description_from_string("Linux Libertine O Bold 9");
    style->title.colour.r    = 0.9;
    style->title.colour.g    = 0.9;
    style->title.colour.b    = 0.9;
    style->title.colour.a    = 1.0;

    /* message */
    style->message.set = TRUE;
    style->message.spacing     = 5;
    style->message.max_lines   = 10;
    style->message.font        = pango_font_description_from_string("Linux Libertine O 9");
    style->message.colour.r    = 0.9;
    style->message.colour.g    = 0.9;
    style->message.colour.b    = 0.9;
    style->message.colour.a    = 1.0;
}

EventdNdStyle *
eventd_nd_style_new(EventdNdStyle *parent)
{
    EventdNdStyle *style;

    style = g_new0(EventdNdStyle, 1);

    if ( parent == NULL )
        _eventd_nd_style_init_defaults(style);
    else
        style->parent = parent;

    return style;
}

void
eventd_nd_style_update(EventdNdStyle *self, GKeyFile *config_file, gint *images_max_width, gint *images_max_height)
{
    self->template = libeventd_nd_notification_template_new(config_file);

    if ( g_key_file_has_group(config_file, "NotificationBubble") )
    {
        Int min_width;
        Int max_width;
        Int padding;
        Int radius;
        gint r_colour;
        Colour colour;

        if ( ( libeventd_config_key_file_get_int(config_file, "NotificationBubble", "MinWidth", &min_width) == 0 )
             || ( libeventd_config_key_file_get_int(config_file, "NotificationBubble", "MaxWidth", &max_width) == 0 )
             || ( libeventd_config_key_file_get_int(config_file, "NotificationBubble", "Padding", &padding) == 0 )
             || ( libeventd_config_key_file_get_int(config_file, "NotificationBubble", "Radius", &radius) == 0 )
             || ( ( r_colour = libeventd_config_key_file_get_colour(config_file, "NotificationBubble", "Colour", &colour) ) == 0 ) )
        {
            self->bubble.set = TRUE;
            self->bubble.min_width = min_width.set ? min_width.value : eventd_nd_style_get_bubble_min_width(self->parent);
            self->bubble.max_width = max_width.set ? max_width.value : eventd_nd_style_get_bubble_max_width(self->parent);
            self->bubble.padding = padding.set ? padding.value : eventd_nd_style_get_bubble_padding(self->parent);
            self->bubble.radius = radius.set ? radius.value : eventd_nd_style_get_bubble_radius(self->parent);
            self->bubble.colour = ( r_colour == 0 ) ? colour : eventd_nd_style_get_bubble_colour(self->parent);
        }
    }

    if ( g_key_file_has_group(config_file, "NotificationTitle") )
    {
        gchar *font;
        gint r_colour;
        Colour colour;

        if ( ( libeventd_config_key_file_get_string(config_file, "NotificationTitle", "Font", &font) == 0 )
             || ( ( r_colour = libeventd_config_key_file_get_colour(config_file, "NotificationTitle", "Colour", &colour) ) == 0 ) )
        {
            pango_font_description_free(self->title.font);

            self->title.set = TRUE;
            self->title.font = ( font != NULL ) ? pango_font_description_from_string(font) : pango_font_description_copy_static(eventd_nd_style_get_title_font(self->parent));
            self->title.colour = ( r_colour == 0 ) ? colour : eventd_nd_style_get_title_colour(self->parent);
        }
    }

    if ( g_key_file_has_group(config_file, "NotificationMessage") )
    {
        Int spacing;
        Int max_lines;
        gchar *font;
        gint r_colour;
        Colour colour;

        if ( ( libeventd_config_key_file_get_int(config_file, "NotificationMessage", "Spacing", &spacing) == 0 )
             || ( libeventd_config_key_file_get_int(config_file, "NotificationMessage", "MaxLines", &max_lines) == 0 )
             || ( libeventd_config_key_file_get_string(config_file, "NotificationMessage", "Font", &font) == 0 )
             || ( ( r_colour = libeventd_config_key_file_get_colour(config_file, "NotificationMessage", "Colour", &colour) ) == 0 ) )
        {
            pango_font_description_free(self->message.font);

            self->message.set = TRUE;
            self->message.spacing = spacing.set ? spacing.value : eventd_nd_style_get_message_spacing(self->parent);
            self->message.max_lines = max_lines.set ? max_lines.value : eventd_nd_style_get_message_max_lines(self->parent);
            self->message.font = ( font != NULL ) ? pango_font_description_from_string(font) : pango_font_description_copy_static(eventd_nd_style_get_message_font(self->parent));
            self->message.colour = ( r_colour == 0 ) ? colour : eventd_nd_style_get_message_colour(self->parent);
        }

    }

    if ( g_key_file_has_group(config_file, "NotificationImage") )
    {
        Int max_width;
        Int max_height;
        Int margin;

        if ( ( libeventd_config_key_file_get_int(config_file, "NotificationImage", "MaxWidth", &max_width) == 0 )
             || ( libeventd_config_key_file_get_int(config_file, "NotificationImage", "MaxHeight", &max_height) == 0 )
             || ( libeventd_config_key_file_get_int(config_file, "NotificationImage", "Margin", &margin) == 0 ) )
        {
            self->image.set = TRUE;
            self->image.max_width = max_width.set ? max_width.value : eventd_nd_style_get_image_max_width(self->parent);
            self->image.max_height = max_height.set ? max_height.value : eventd_nd_style_get_image_max_height(self->parent);
            self->image.margin = margin.set ? margin.value : eventd_nd_style_get_image_margin(self->parent);

            if ( max_width.set )
                *images_max_width  = MAX(*images_max_width, max_width.value);
            if ( max_height.set )
                *images_max_height  = MAX(*images_max_height, max_height.value);
        }

    }

    if ( g_key_file_has_group(config_file, "NotificationIcon") )
    {
        gchar *placement;
        gchar *anchor;
        Int max_width;
        Int max_height;
        Int margin;
        Int fade_width;

        if ( ( libeventd_config_key_file_get_string(config_file, "NotificationIcon", "Placement", &placement) == 0 )
             || ( libeventd_config_key_file_get_string(config_file, "NotificationIcon", "Anchor", &anchor) == 0 )
             || ( libeventd_config_key_file_get_int(config_file, "NotificationIcon", "MaxWidth", &max_width) == 0 )
             || ( libeventd_config_key_file_get_int(config_file, "NotificationIcon", "MaxHeight", &max_height) == 0 )
             || ( libeventd_config_key_file_get_int(config_file, "NotificationIcon", "Margin", &margin) == 0 )
             || ( libeventd_config_key_file_get_int(config_file, "NotificationIcon", "FadeWidth", &fade_width) == 0 ) )
        {
            self->icon.set = TRUE;

            if ( placement != NULL )
            {
                if ( g_strcmp0(placement, "Background") == 0 )
                    self->icon.placement = EVENTD_ND_STYLE_ICON_PLACEMENT_BACKGROUND;
                else if ( g_strcmp0(placement, "Overlay") == 0 )
                    self->icon.placement = EVENTD_ND_STYLE_ICON_PLACEMENT_OVERLAY;
                else if ( g_strcmp0(placement, "Foreground") == 0 )
                    self->icon.placement = EVENTD_ND_STYLE_ICON_PLACEMENT_FOREGROUND;
                else
                    g_warning("Wrong placement value '%s'", placement);
                g_free(placement);
            }
            else
                self->icon.placement = eventd_nd_style_get_icon_placement(self->parent);

            if ( anchor != NULL )
            {
                if ( g_strcmp0(anchor, "Top") == 0 )
                    self->icon.anchor = EVENTD_ND_ANCHOR_TOP;
                else if ( g_strcmp0(anchor, "Bottom") == 0 )
                    self->icon.anchor = EVENTD_ND_ANCHOR_BOTTOM;
                else if ( g_strcmp0(anchor, "Center") == 0 )
                    self->icon.anchor = EVENTD_ND_ANCHOR_VCENTER;
                else
                    g_warning("Wrong anchor value '%s'", anchor);
                g_free(anchor);
            }
            else
                self->icon.anchor = eventd_nd_style_get_icon_anchor(self->parent);

            self->icon.max_width = max_width.set ? max_width.value : eventd_nd_style_get_icon_max_width(self->parent);
            self->icon.max_height = max_height.set ? max_height.value : eventd_nd_style_get_icon_max_height(self->parent);
            self->icon.margin = margin.set ? margin.value : eventd_nd_style_get_icon_margin(self->parent);

            if ( fade_width.set )
            {
                if ( ( fade_width.value < 0 ) || ( fade_width.value > 100 ) )
                    g_warning("Wrong percentage value '%jd'", fade_width.value);
                else
                    self->icon.fade_width = (gdouble) fade_width.value / 100.;

                if ( max_width.set )
                    *images_max_width  = MAX(*images_max_width, max_width.value);
                if ( max_height.set )
                    *images_max_height  = MAX(*images_max_height, max_height.value);
            }
            else
                self->icon.fade_width = eventd_nd_style_get_icon_fade_width(self->parent);
        }
    }
}

void
eventd_nd_style_free(gpointer data)
{
    EventdNdStyle *style = data;
    if ( style == NULL )
        return;

    pango_font_description_free(style->title.font);
    pango_font_description_free(style->message.font);

    libeventd_nd_notification_template_free(style->template);

    g_free(style);
}


LibeventdNdNotificationTemplate *
eventd_nd_style_get_template(EventdNdStyle *self)
{
    return self->template;
}

gint
eventd_nd_style_get_bubble_min_width(EventdNdStyle *self)
{
    if ( self->bubble.set )
        return self->bubble.min_width;
    return eventd_nd_style_get_bubble_min_width(self->parent);
}

gint
eventd_nd_style_get_bubble_max_width(EventdNdStyle *self)
{
    if ( self->bubble.set )
        return self->bubble.max_width;
    return eventd_nd_style_get_bubble_max_width(self->parent);
}

gint
eventd_nd_style_get_bubble_padding(EventdNdStyle *self)
{
    if ( self->bubble.set )
        return self->bubble.padding;
    return eventd_nd_style_get_bubble_padding(self->parent);
}

gint
eventd_nd_style_get_bubble_radius(EventdNdStyle *self)
{
    if ( self->bubble.set )
        return self->bubble.radius;
    return eventd_nd_style_get_bubble_radius(self->parent);
}

Colour
eventd_nd_style_get_bubble_colour(EventdNdStyle *self)
{
    if ( self->bubble.set )
        return self->bubble.colour;
    return eventd_nd_style_get_bubble_colour(self->parent);
}

gint
eventd_nd_style_get_image_max_width(EventdNdStyle *self)
{
    if ( self->image.set )
        return self->image.max_width;
    return eventd_nd_style_get_image_max_width(self->parent);
}

gint
eventd_nd_style_get_image_max_height(EventdNdStyle *self)
{
    if ( self->image.set )
        return self->image.max_height;
    return eventd_nd_style_get_image_max_height(self->parent);
}

gint
eventd_nd_style_get_image_margin(EventdNdStyle *self)
{
    if ( self->image.set )
        return self->image.margin;
    return eventd_nd_style_get_image_margin(self->parent);
}

EventdNdStyleIconPlacement
eventd_nd_style_get_icon_placement(EventdNdStyle *self)
{
    if ( self->icon.set )
        return self->icon.placement;
    return eventd_nd_style_get_icon_placement(self->parent);
}

EventdNdAnchor
eventd_nd_style_get_icon_anchor(EventdNdStyle *self)
{
    if ( self->icon.set )
        return self->icon.anchor;
    return eventd_nd_style_get_icon_anchor(self->parent);
}

gint
eventd_nd_style_get_icon_max_width(EventdNdStyle *self)
{
    if ( self->icon.set )
        return self->icon.max_width;
    return eventd_nd_style_get_icon_max_width(self->parent);
}

gint
eventd_nd_style_get_icon_max_height(EventdNdStyle *self)
{
    if ( self->icon.set )
        return self->icon.max_height;
    return eventd_nd_style_get_icon_max_height(self->parent);
}

gint
eventd_nd_style_get_icon_margin(EventdNdStyle *self)
{
    if ( self->icon.set )
        return self->icon.margin;
    return eventd_nd_style_get_icon_margin(self->parent);
}

gdouble
eventd_nd_style_get_icon_fade_width(EventdNdStyle *self)
{
    if ( self->icon.set )
        return self->icon.fade_width;
    return eventd_nd_style_get_icon_fade_width(self->parent);
}

const PangoFontDescription *
eventd_nd_style_get_title_font(EventdNdStyle *self)
{
    if ( self->title.set )
        return self->title.font;
    return eventd_nd_style_get_title_font(self->parent);
}

Colour
eventd_nd_style_get_title_colour(EventdNdStyle *self)
{
    if ( self->title.set )
        return self->title.colour;
    return eventd_nd_style_get_title_colour(self->parent);
}

gint
eventd_nd_style_get_message_spacing(EventdNdStyle *self)
{
    if ( self->message.set )
        return self->message.spacing;
    return eventd_nd_style_get_message_spacing(self->parent);
}

guint8
eventd_nd_style_get_message_max_lines(EventdNdStyle *self)
{
    if ( self->message.set )
        return self->message.max_lines;
    return eventd_nd_style_get_message_max_lines(self->parent);
}

const PangoFontDescription *
eventd_nd_style_get_message_font(EventdNdStyle *self)
{
    if ( self->message.set )
        return self->message.font;
    return eventd_nd_style_get_message_font(self->parent);
}

Colour
eventd_nd_style_get_message_colour(EventdNdStyle *self)
{
    if ( self->message.set )
        return self->message.colour;
    return eventd_nd_style_get_message_colour(self->parent);
}
