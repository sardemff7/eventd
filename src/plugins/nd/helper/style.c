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

#include <eventd-nd-style.h>

struct _EventdNdStyle {
    gint bubble_min_width;
    gint bubble_max_width;

    gint bubble_padding;
    gint bubble_radius;
    Colour bubble_colour;

    gint image_max_width;
    gint image_max_height;
    gint image_margin;

    EventdNdStyleIconPlacement icon_placement;
    EventdNdAnchor icon_anchor;
    gint icon_max_width;
    gint icon_max_height;
    gint icon_margin;

    gchar *title_font;
    Colour title_colour;

    gint message_spacing;
    guint8 message_max_lines;
    gchar *message_font;
    Colour message_colour;
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
    style->bubble_min_width = 200;
    style->bubble_max_width = -1;

    /* bubble style */
    style->bubble_padding   = 10;
    style->bubble_radius    = 10;
    style->bubble_colour.r = 0.15;
    style->bubble_colour.g = 0.15;
    style->bubble_colour.b = 0.15;
    style->bubble_colour.a = 1.0;

    /* image */
    style->image_max_width  = 50;
    style->image_max_height = 50;
    style->image_margin     = 10;

    /* icon */
    style->icon_placement  = EVENTD_ND_STYLE_ICON_PLACEMENT_OVERLAY;
    style->icon_anchor     = EVENTD_ND_ANCHOR_VCENTER;
    style->icon_max_width  = 25;
    style->icon_max_height = 25;
    style->icon_margin     = 10;

    /* title */
    style->title_font = g_strdup("Linux Libertine O Bold 9");
    style->title_colour.r    = 0.9;
    style->title_colour.g    = 0.9;
    style->title_colour.b    = 0.9;
    style->title_colour.a    = 1.0;

    /* message */
    style->message_spacing     = 5;
    style->message_max_lines   = 10;
    style->message_font = g_strdup("Linux Libertine O 9");
    style->message_colour.r    = 0.9;
    style->message_colour.g    = 0.9;
    style->message_colour.b    = 0.9;
    style->message_colour.a    = 1.0;
}

static void
_eventd_nd_style_init_parent(EventdNdStyle *self, EventdNdStyle *parent)
{
    /* bubble geometry */
    self->bubble_min_width = parent->bubble_min_width;
    self->bubble_max_width = parent->bubble_max_width;

    /* bubble style */
    self->bubble_padding = parent->bubble_padding;
    self->bubble_radius  = parent->bubble_radius;
    self->bubble_colour  = parent->bubble_colour;

    /* image */
    self->image_max_width  = parent->image_max_width;
    self->image_max_height = parent->image_max_height;
    self->image_margin     = parent->image_margin;

    /* icon */
    self->icon_placement  = parent->icon_placement;
    self->icon_anchor     = parent->icon_anchor;
    self->icon_max_width  = parent->icon_max_width;
    self->icon_max_height = parent->icon_max_height;
    self->icon_margin     = parent->icon_margin;

    /* title */
    self->title_font   = g_strdup(parent->title_font);
    self->title_colour = parent->title_colour;

    /* message */
    self->message_spacing   = parent->message_spacing;
    self->message_max_lines = parent->message_max_lines;
    self->message_font      = g_strdup(parent->message_font);
    self->message_colour    = parent->message_colour;
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

    if ( g_key_file_has_group(config_file, "bubble") )
    {
        if ( libeventd_config_key_file_get_int(config_file, "bubble", "min-width", &integer) == 0 )
            style->bubble_min_width = integer.value;

        if ( libeventd_config_key_file_get_int(config_file, "bubble", "max-width", &integer) == 0 )
            style->bubble_max_width = integer.value;

        if ( libeventd_config_key_file_get_int(config_file, "bubble", "padding", &integer) == 0 )
            style->bubble_padding = integer.value;

        if ( libeventd_config_key_file_get_int(config_file, "bubble", "radius", &integer) == 0 )
            style->bubble_radius = integer.value;

        if ( ( libeventd_config_key_file_get_string(config_file, "bubble", "colour", &string) == 0 ) && _eventd_nd_style_parse_colour(string, &colour) )
        {
            g_free(string);
            style->bubble_colour = colour;
        }
    }

    if ( g_key_file_has_group(config_file, "title") )
    {
        if ( libeventd_config_key_file_get_string(config_file, "title", "font", &string) == 0 )
        {
            g_free(style->title_font);
            style->title_font = string;
        }

        if ( ( libeventd_config_key_file_get_string(config_file, "title", "colour", &string) == 0 ) && _eventd_nd_style_parse_colour(string, &colour) )
        {
            g_free(string);
            style->title_colour = colour;
        }
    }

    if ( g_key_file_has_group(config_file, "message") )
    {
        if ( libeventd_config_key_file_get_int(config_file, "message", "spacing", &integer) == 0 )
            style->message_spacing = integer.value;

        if ( libeventd_config_key_file_get_int(config_file, "message", "max-lines", &integer) == 0 )
            style->message_max_lines = integer.value;

        if ( libeventd_config_key_file_get_string(config_file, "message", "font", &string) == 0 )
        {
            g_free(style->message_font);
            style->message_font = string;
        }

        if ( ( libeventd_config_key_file_get_string(config_file, "message", "colour", &string) == 0 ) && _eventd_nd_style_parse_colour(string, &colour) )
        {
            g_free(string);
            style->message_colour = colour;
        }
    }

    if ( g_key_file_has_group(config_file, "image") )
    {
        if ( libeventd_config_key_file_get_int(config_file, "image", "max-width", &integer) == 0 )
            style->image_max_width = integer.value;

        if ( libeventd_config_key_file_get_int(config_file, "image", "max-height", &integer) == 0 )
            style->image_max_height = integer.value;

        if ( libeventd_config_key_file_get_int(config_file, "image", "margin", &integer) == 0 )
            style->image_margin = integer.value;
    }

    if ( g_key_file_has_group(config_file, "icon") )
    {
        if ( libeventd_config_key_file_get_string(config_file, "icon", "placement", &string) == 0 )
        {
            if ( g_strcmp0(string, "overlay") == 0 )
                style->icon_placement = EVENTD_ND_STYLE_ICON_PLACEMENT_OVERLAY;
            else if ( g_strcmp0(string, "foreground") == 0 )
                style->icon_placement = EVENTD_ND_STYLE_ICON_PLACEMENT_FOREGROUND;
            else
                g_warning("Wrong placement value '%s'", string);
            g_free(string);
        }

        if ( libeventd_config_key_file_get_string(config_file, "icon", "anchor", &string) == 0 )
        {
            if ( g_strcmp0(string, "top") == 0 )
                style->icon_anchor = EVENTD_ND_ANCHOR_TOP;
            else if ( g_strcmp0(string, "bottom") == 0 )
                style->icon_anchor = EVENTD_ND_ANCHOR_BOTTOM;
            else if ( g_strcmp0(string, "center") == 0 )
                style->icon_anchor = EVENTD_ND_ANCHOR_VCENTER;
            else
                g_warning("Wrong anchor value '%s'", string);
            g_free(string);
        }

        if ( libeventd_config_key_file_get_int(config_file, "icon", "max-width", &integer) == 0 )
            style->icon_max_width = integer.value;

        if ( libeventd_config_key_file_get_int(config_file, "icon", "max-height", &integer) == 0 )
            style->icon_max_height = integer.value;

        if ( libeventd_config_key_file_get_int(config_file, "icon", "margin", &integer) == 0 )
            style->icon_margin = integer.value;
    }
}

void
eventd_nd_style_free(EventdNdStyle *style)
{
    if ( style == NULL )
        return;

    g_free(style->title_font);
    g_free(style->message_font);

    g_free(style);
}

gint
eventd_nd_style_get_bubble_min_width(EventdNdStyle *self)
{
    return self->bubble_min_width;
}

gint
eventd_nd_style_get_bubble_max_width(EventdNdStyle *self)
{
    return self->bubble_max_width;
}

gint
eventd_nd_style_get_bubble_padding(EventdNdStyle *self)
{
    return self->bubble_padding;
}

gint
eventd_nd_style_get_bubble_radius(EventdNdStyle *self)
{
    return self->bubble_radius;
}

Colour
eventd_nd_style_get_bubble_colour(EventdNdStyle *self)
{
    return self->bubble_colour;
}

gint
eventd_nd_style_get_image_max_width(EventdNdStyle *self)
{
    return self->image_max_width;
}

gint
eventd_nd_style_get_image_max_height(EventdNdStyle *self)
{
    return self->image_max_height;
}

gint
eventd_nd_style_get_image_margin(EventdNdStyle *self)
{
    return self->image_margin;
}

EventdNdStyleIconPlacement
eventd_nd_style_get_icon_placement(EventdNdStyle *self)
{
    return self->icon_placement;
}

EventdNdAnchor
eventd_nd_style_get_icon_anchor(EventdNdStyle *self)
{
    return self->icon_anchor;
}

gint
eventd_nd_style_get_icon_max_width(EventdNdStyle *self)
{
    return self->icon_max_width;
}

gint
eventd_nd_style_get_icon_max_height(EventdNdStyle *self)
{
    return self->icon_max_height;
}

gint
eventd_nd_style_get_icon_margin(EventdNdStyle *self)
{
    return self->icon_margin;
}

const gchar *
eventd_nd_style_get_title_font(EventdNdStyle *self)
{
    return self->title_font;
}

Colour
eventd_nd_style_get_title_colour(EventdNdStyle *self)
{
    return self->title_colour;
}

gint
eventd_nd_style_get_message_spacing(EventdNdStyle *self)
{
    return self->message_spacing;
}

guint8
eventd_nd_style_get_message_max_lines(EventdNdStyle *self)
{
    return self->message_max_lines;
}

const gchar *
eventd_nd_style_get_message_font(EventdNdStyle *self)
{
    return self->message_font;
}

Colour
eventd_nd_style_get_message_colour(EventdNdStyle *self)
{
    return self->message_colour;
}
