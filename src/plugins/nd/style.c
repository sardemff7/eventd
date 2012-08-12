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

#include <libeventd-config.h>

#include "style.h"

struct _EventdNdStyle {
    struct {
        gint   min_width;
        gint   max_width;

        gint   padding;
        gint   radius;
        Colour colour;
    } bubble;

    struct {
        gint max_width;
        gint max_height;
        gint margin;
    } image;

    struct {
        EventdNdStyleIconPlacement placement;
        EventdNdAnchor             anchor;
        gint                       max_width;
        gint                       max_height;
        gint                       margin;
        gdouble                    fade_width;
    } icon;

    struct {
        gchar *font;
        Colour colour;
    } title;

    struct {
        gint   spacing;
        guint8 max_lines;
        gchar *font;
        Colour colour;
    } message;
};

static gboolean
_eventd_nd_style_parse_colour(const gchar *string, Colour *colour)
{
    if ( string[0] == '#' )
    {
        gchar hex[3] = {0};

        hex[0] = string[1]; hex[1] = string[2];
        colour->r = g_ascii_strtoll(hex, NULL, 16) / 255.;

        hex[0] = string[3]; hex[1] = string[4];
        colour->g = g_ascii_strtoll(hex, NULL, 16) / 255.;

        hex[0] = string[5]; hex[1] = string[6];
        colour->b = g_ascii_strtoll(hex, NULL, 16) / 255.;

        if ( string[7] != 0 )
        {
            hex[0] = string[7]; hex[1] = string[8];
            colour->a = g_ascii_strtoll(hex, NULL, 16) / 255.;
        }
        else
            colour->a = 1.0;

        return TRUE;
    }
    else if ( g_str_has_prefix(string, "rgb(") && g_str_has_suffix(string, ")") )
        g_warning("rgba() format not yet supported");

    return FALSE;
}

static void
_eventd_nd_style_init_defaults(EventdNdStyle *style)
{
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
    style->image.max_width  = 50;
    style->image.max_height = 50;
    style->image.margin     = 10;

    /* icon */
    style->icon.placement  = EVENTD_ND_STYLE_ICON_PLACEMENT_BACKGROUND;
    style->icon.anchor     = EVENTD_ND_ANCHOR_VCENTER;
    style->icon.max_width  = 50;
    style->icon.max_height = 50;
    style->icon.margin     = 10;
    style->icon.fade_width = 0.75;

    /* title */
    style->title.font = g_strdup("Linux Libertine O Bold 9");
    style->title.colour.r    = 0.9;
    style->title.colour.g    = 0.9;
    style->title.colour.b    = 0.9;
    style->title.colour.a    = 1.0;

    /* message */
    style->message.spacing     = 5;
    style->message.max_lines   = 10;
    style->message.font = g_strdup("Linux Libertine O 9");
    style->message.colour.r    = 0.9;
    style->message.colour.g    = 0.9;
    style->message.colour.b    = 0.9;
    style->message.colour.a    = 1.0;
}

static void
_eventd_nd_style_init_parent(EventdNdStyle *self, EventdNdStyle *parent)
{
    /* bubble geometry */
    self->bubble.min_width = parent->bubble.min_width;
    self->bubble.max_width = parent->bubble.max_width;

    /* bubble style */
    self->bubble.padding = parent->bubble.padding;
    self->bubble.radius  = parent->bubble.radius;
    self->bubble.colour  = parent->bubble.colour;

    /* image */
    self->image.max_width  = parent->image.max_width;
    self->image.max_height = parent->image.max_height;
    self->image.margin     = parent->image.margin;

    /* icon */
    self->icon.placement  = parent->icon.placement;
    self->icon.anchor     = parent->icon.anchor;
    self->icon.max_width  = parent->icon.max_width;
    self->icon.max_height = parent->icon.max_height;
    self->icon.margin     = parent->icon.margin;
    self->icon.fade_width = parent->icon.fade_width;

    /* title */
    self->title.font   = g_strdup(parent->title.font);
    self->title.colour = parent->title.colour;

    /* message */
    self->message.spacing   = parent->message.spacing;
    self->message.max_lines = parent->message.max_lines;
    self->message.font      = g_strdup(parent->message.font);
    self->message.colour    = parent->message.colour;
}

EventdNdStyle *
eventd_nd_style_new(EventdNdStyle *parent)
{
    EventdNdStyle *style;

    style = g_new0(EventdNdStyle, 1);

    if ( parent == NULL )
        _eventd_nd_style_init_defaults(style);
    else
        _eventd_nd_style_init_parent(style, parent);

    return style;
}

void
eventd_nd_style_update(EventdNdStyle *style, GKeyFile *config_file)
{
    gchar *string;
    Int integer;
    Colour colour;

    if ( g_key_file_has_group(config_file, "NotificationBubble") )
    {
        if ( libeventd_config_key_file_get_int(config_file, "NotificationBubble", "MinWidth", &integer) == 0 )
            style->bubble.min_width = integer.value;

        if ( libeventd_config_key_file_get_int(config_file, "NotificationBubble", "MaxWidth", &integer) == 0 )
            style->bubble.max_width = integer.value;

        if ( libeventd_config_key_file_get_int(config_file, "NotificationBubble", "Padding", &integer) == 0 )
            style->bubble.padding = integer.value;

        if ( libeventd_config_key_file_get_int(config_file, "NotificationBubble", "Radius", &integer) == 0 )
            style->bubble.radius = integer.value;

        if ( ( libeventd_config_key_file_get_string(config_file, "NotificationBubble", "Colour", &string) == 0 ) && _eventd_nd_style_parse_colour(string, &colour) )
        {
            g_free(string);
            style->bubble.colour = colour;
        }
    }

    if ( g_key_file_has_group(config_file, "NotificationTitle") )
    {
        if ( libeventd_config_key_file_get_string(config_file, "NotificationTitle", "Font", &string) == 0 )
        {
            g_free(style->title.font);
            style->title.font = string;
        }

        if ( ( libeventd_config_key_file_get_string(config_file, "NotificationTitle", "Colour", &string) == 0 ) && _eventd_nd_style_parse_colour(string, &colour) )
        {
            g_free(string);
            style->title.colour = colour;
        }
    }

    if ( g_key_file_has_group(config_file, "NotificationMessage") )
    {
        if ( libeventd_config_key_file_get_int(config_file, "NotificationMessage", "Spacing", &integer) == 0 )
            style->message.spacing = integer.value;

        if ( libeventd_config_key_file_get_int(config_file, "NotificationMessage", "MaxLines", &integer) == 0 )
            style->message.max_lines = integer.value;

        if ( libeventd_config_key_file_get_string(config_file, "NotificationMessage", "Font", &string) == 0 )
        {
            g_free(style->message.font);
            style->message.font = string;
        }

        if ( ( libeventd_config_key_file_get_string(config_file, "NotificationMessage", "Colour", &string) == 0 ) && _eventd_nd_style_parse_colour(string, &colour) )
        {
            g_free(string);
            style->message.colour = colour;
        }
    }

    if ( g_key_file_has_group(config_file, "NotificationImage") )
    {
        if ( libeventd_config_key_file_get_int(config_file, "NotificationImage", "MaxWidth", &integer) == 0 )
            style->image.max_width = integer.value;

        if ( libeventd_config_key_file_get_int(config_file, "NotificationImage", "MaxHeight", &integer) == 0 )
            style->image.max_height = integer.value;

        if ( libeventd_config_key_file_get_int(config_file, "NotificationImage", "Margin", &integer) == 0 )
            style->image.margin = integer.value;
    }

    if ( g_key_file_has_group(config_file, "NotificationIcon") )
    {
        if ( libeventd_config_key_file_get_string(config_file, "NotificationIcon", "Placement", &string) == 0 )
        {
            if ( g_strcmp0(string, "Background") == 0 )
                style->icon.placement = EVENTD_ND_STYLE_ICON_PLACEMENT_BACKGROUND;
            else if ( g_strcmp0(string, "Overlay") == 0 )
                style->icon.placement = EVENTD_ND_STYLE_ICON_PLACEMENT_OVERLAY;
            else if ( g_strcmp0(string, "Foreground") == 0 )
                style->icon.placement = EVENTD_ND_STYLE_ICON_PLACEMENT_FOREGROUND;
            else
                g_warning("Wrong placement value '%s'", string);
            g_free(string);
        }

        if ( libeventd_config_key_file_get_string(config_file, "NotificationIcon", "Anchor", &string) == 0 )
        {
            if ( g_strcmp0(string, "Top") == 0 )
                style->icon.anchor = EVENTD_ND_ANCHOR_TOP;
            else if ( g_strcmp0(string, "Bottom") == 0 )
                style->icon.anchor = EVENTD_ND_ANCHOR_BOTTOM;
            else if ( g_strcmp0(string, "Center") == 0 )
                style->icon.anchor = EVENTD_ND_ANCHOR_VCENTER;
            else
                g_warning("Wrong anchor value '%s'", string);
            g_free(string);
        }

        if ( libeventd_config_key_file_get_int(config_file, "NotificationIcon", "MaxWidth", &integer) == 0 )
            style->icon.max_width = integer.value;

        if ( libeventd_config_key_file_get_int(config_file, "NotificationIcon", "MaxHeight", &integer) == 0 )
            style->icon.max_height = integer.value;

        if ( libeventd_config_key_file_get_int(config_file, "NotificationIcon", "Margin", &integer) == 0 )
            style->icon.margin = integer.value;

        if ( libeventd_config_key_file_get_int(config_file, "NotificationIcon", "FadeWidth", &integer) == 0 )
        {
            if ( ( integer.value < 0 ) || ( integer.value > 100 ) )
                g_warning("Wrong percentage value '%jd'", integer.value);
            else
                style->icon.fade_width = (gdouble) integer.value / 100.;
        }
    }
}

void
eventd_nd_style_free(EventdNdStyle *style)
{
    if ( style == NULL )
        return;

    g_free(style->title.font);
    g_free(style->message.font);

    g_free(style);
}

gint
eventd_nd_style_get_bubble_min_width(EventdNdStyle *self)
{
    return self->bubble.min_width;
}

gint
eventd_nd_style_get_bubble_max_width(EventdNdStyle *self)
{
    return self->bubble.max_width;
}

gint
eventd_nd_style_get_bubble_padding(EventdNdStyle *self)
{
    return self->bubble.padding;
}

gint
eventd_nd_style_get_bubble_radius(EventdNdStyle *self)
{
    return self->bubble.radius;
}

Colour
eventd_nd_style_get_bubble_colour(EventdNdStyle *self)
{
    return self->bubble.colour;
}

gint
eventd_nd_style_get_image_max_width(EventdNdStyle *self)
{
    return self->image.max_width;
}

gint
eventd_nd_style_get_image_max_height(EventdNdStyle *self)
{
    return self->image.max_height;
}

gint
eventd_nd_style_get_image_margin(EventdNdStyle *self)
{
    return self->image.margin;
}

EventdNdStyleIconPlacement
eventd_nd_style_get_icon_placement(EventdNdStyle *self)
{
    return self->icon.placement;
}

EventdNdAnchor
eventd_nd_style_get_icon_anchor(EventdNdStyle *self)
{
    return self->icon.anchor;
}

gint
eventd_nd_style_get_icon_max_width(EventdNdStyle *self)
{
    return self->icon.max_width;
}

gint
eventd_nd_style_get_icon_max_height(EventdNdStyle *self)
{
    return self->icon.max_height;
}

gint
eventd_nd_style_get_icon_margin(EventdNdStyle *self)
{
    return self->icon.margin;
}

gdouble
eventd_nd_style_get_icon_fade_width(EventdNdStyle *self)
{
    return self->icon.fade_width;
}

const gchar *
eventd_nd_style_get_title_font(EventdNdStyle *self)
{
    return self->title.font;
}

Colour
eventd_nd_style_get_title_colour(EventdNdStyle *self)
{
    return self->title.colour;
}

gint
eventd_nd_style_get_message_spacing(EventdNdStyle *self)
{
    return self->message.spacing;
}

guint8
eventd_nd_style_get_message_max_lines(EventdNdStyle *self)
{
    return self->message.max_lines;
}

const gchar *
eventd_nd_style_get_message_font(EventdNdStyle *self)
{
    return self->message.font;
}

Colour
eventd_nd_style_get_message_colour(EventdNdStyle *self)
{
    return self->message.colour;
}
