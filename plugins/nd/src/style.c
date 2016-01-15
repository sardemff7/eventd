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

#include <glib.h>
#include <pango/pango.h>

#include <libeventd-event.h>
#include <libeventd-helpers-config.h>

#include "style.h"

const gchar * const eventd_nd_anchors[_EVENTD_ND_ANCHOR_SIZE] = {
    [EVENTD_ND_ANCHOR_TOP_LEFT]     = "top left",
    [EVENTD_ND_ANCHOR_TOP]          = "top",
    [EVENTD_ND_ANCHOR_TOP_RIGHT]    = "top right",
    [EVENTD_ND_ANCHOR_BOTTOM_LEFT]  = "bottom left",
    [EVENTD_ND_ANCHOR_BOTTOM]       = "bottom",
    [EVENTD_ND_ANCHOR_BOTTOM_RIGHT] = "bottom right",
};

static const gchar * const _eventd_nd_style_pango_alignments[] = {
    [PANGO_ALIGN_LEFT]   = "left",
    [PANGO_ALIGN_RIGHT]  = "right",
    [PANGO_ALIGN_CENTER] = "center",
};

static const gchar * const _eventd_nd_style_anchors_vertical[] = {
    [EVENTD_ND_VANCHOR_TOP]     = "top",
    [EVENTD_ND_VANCHOR_BOTTOM]  = "bottom",
    [EVENTD_ND_VANCHOR_CENTER] = "center",
};

static const gchar * const _eventd_nd_style_anchors_horizontal[] = {
    [EVENTD_ND_HANCHOR_LEFT]    = "left",
    [EVENTD_ND_HANCHOR_RIGHT]   = "right",
    [EVENTD_ND_HANCHOR_CENTER] = "center",
};

static const gchar * const _eventd_nd_style_icon_placements[] = {
    [EVENTD_ND_STYLE_ICON_PLACEMENT_BACKGROUND] = "background",
    [EVENTD_ND_STYLE_ICON_PLACEMENT_OVERLAY]    = "overlay",
    [EVENTD_ND_STYLE_ICON_PLACEMENT_FOREGROUND] = "foreground",
};

struct _EventdPluginAction {
    EventdNdStyle *parent;

    struct {
        gboolean set;

        FormatString *text;
        Filename *image;
        Filename *icon;
    } template;

    struct {
        gboolean set;

        EventdNdAnchor anchor;

        gint timeout;

        gint   min_width;
        gint   max_width;

        gint   padding;
        gint   radius;
        Colour colour;
    } bubble;

    struct {
        gboolean set;

        EventdNdAnchorVertical anchor;
        gint                   max_width;
        gint                   max_height;
        gint                   margin;
    } image;

    struct {
        gboolean set;

        EventdNdStyleIconPlacement placement;
        EventdNdAnchorVertical     anchor;
        gint                       max_width;
        gint                       max_height;
        gint                       margin;
        gdouble                    fade_width;
    } icon;

    struct {
        gboolean set;

        PangoFontDescription *font;
        PangoAlignment align;
        guint8 max_lines;
        Colour colour;
    } text;
};

static void
_eventd_nd_style_init_defaults(EventdNdStyle *style)
{
    /* template */
    style->template.set     = TRUE;
    style->template.text   = evhelpers_format_string_new(g_strdup("<b>${title}</b>${message/^/\n}"));
    style->template.image   = evhelpers_filename_new(g_strdup("image"));
    style->template.icon    = evhelpers_filename_new(g_strdup("icon"));

    /* bubble */
    style->bubble.set = TRUE;

    /* bubble anchor */
    style->bubble.anchor  = EVENTD_ND_ANCHOR_TOP_RIGHT;

    /* bubble timing */
    style->bubble.timeout = 3000;

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

    /* text */
    style->text.set = TRUE;
    style->text.font        = pango_font_description_from_string("Linux Libertine O 9");
    style->text.align       = PANGO_ALIGN_LEFT;
    style->text.max_lines   = 10;
    style->text.colour.r    = 0.9;
    style->text.colour.g    = 0.9;
    style->text.colour.b    = 0.9;
    style->text.colour.a    = 1.0;

    /* image */
    style->image.set = TRUE;
    style->image.anchor     = EVENTD_ND_VANCHOR_TOP;
    style->image.max_width  = 50;
    style->image.max_height = 50;
    style->image.margin     = 10;

    /* icon */
    style->icon.set = TRUE;
    style->icon.placement  = EVENTD_ND_STYLE_ICON_PLACEMENT_BACKGROUND;
    style->icon.anchor     = EVENTD_ND_VANCHOR_CENTER;
    style->icon.max_width  = 25;
    style->icon.max_height = 25;
    style->icon.margin     = 10;
    style->icon.fade_width = 0.75;
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
    if ( g_key_file_has_group(config_file, "Notification") )
    {
        self->template.set = TRUE;

        FormatString *string = NULL;

        if ( evhelpers_config_key_file_get_locale_format_string(config_file, "Notification", "Text", NULL, &string) == 0 )
        {
            evhelpers_format_string_unref(self->template.text);
            self->template.text = string;
        }
        else if ( self->parent != NULL )
        {
            evhelpers_format_string_unref(self->template.text);
            self->template.text = evhelpers_format_string_ref(eventd_nd_style_get_template_text(self->parent));
        }

        Filename *filename = NULL;

        if ( evhelpers_config_key_file_get_filename(config_file, "Notification", "Image", &filename) == 0 )
        {
            evhelpers_filename_unref(self->template.image);
            self->template.image = filename;
        }
        else if ( self->parent != NULL )
        {
            evhelpers_filename_unref(self->template.image);
            self->template.image = evhelpers_filename_ref(eventd_nd_style_get_template_image(self->parent));
        }

        filename = NULL;
        if ( evhelpers_config_key_file_get_filename(config_file, "Notification", "Icon", &filename) == 0 )
        {
            evhelpers_filename_unref(self->template.icon);
            self->template.icon = filename;
        }
        else if ( self->parent != NULL )
        {
            evhelpers_filename_unref(self->template.icon);
            self->template.icon = evhelpers_filename_ref(eventd_nd_style_get_template_icon(self->parent));
        }
    }

    if ( g_key_file_has_group(config_file, "NotificationBubble") )
    {
        self->bubble.set = TRUE;

        guint64 enum_value;
        Int integer;
        Colour colour;

        if ( evhelpers_config_key_file_get_enum(config_file, "NotificationBubble", "Anchor", eventd_nd_anchors, G_N_ELEMENTS(eventd_nd_anchors), &enum_value) == 0 )
            self->bubble.anchor = enum_value;
        else if ( self->parent != NULL )
            self->bubble.anchor = eventd_nd_style_get_bubble_anchor(self->parent);

        if ( evhelpers_config_key_file_get_int(config_file, "NotificationBubble", "Timeout", &integer) == 0 )
            self->bubble.timeout = ( integer.value > 0 ) ? integer.value : 0;
        else if ( self->parent != NULL )
            self->bubble.timeout = eventd_nd_style_get_bubble_timeout(self->parent);

        if ( evhelpers_config_key_file_get_int(config_file, "NotificationBubble", "MinWidth", &integer) == 0 )
            self->bubble.min_width = ( integer.value > 0 ) ? integer.value : 0;
        else if ( self->parent != NULL )
            self->bubble.min_width = eventd_nd_style_get_bubble_min_width(self->parent);

        if ( evhelpers_config_key_file_get_int(config_file, "NotificationBubble", "MaxWidth", &integer) == 0 )
            self->bubble.max_width = integer.value;
        else if ( self->parent != NULL )
            self->bubble.max_width = eventd_nd_style_get_bubble_max_width(self->parent);

        if ( evhelpers_config_key_file_get_int(config_file, "NotificationBubble", "Padding", &integer) == 0 )
            self->bubble.padding = integer.value;
        else if ( self->parent != NULL )
            self->bubble.padding = eventd_nd_style_get_bubble_padding(self->parent);

        if ( evhelpers_config_key_file_get_int(config_file, "NotificationBubble", "Radius", &integer) == 0 )
            self->bubble.radius = integer.value;
        else if ( self->parent != NULL )
            self->bubble.radius = eventd_nd_style_get_bubble_radius(self->parent);

        if ( evhelpers_config_key_file_get_colour(config_file, "NotificationBubble", "Colour", &colour) == 0 )
            self->bubble.colour = colour;
        else if ( self->parent != NULL )
            self->bubble.colour = eventd_nd_style_get_bubble_colour(self->parent);
    }

    if ( g_key_file_has_group(config_file, "NotificationText") )
    {
        self->text.set = TRUE;

        gchar *string;
        guint64 enum_value;
        Int integer;
        Colour colour;

        if ( evhelpers_config_key_file_get_string(config_file, "NotificationText", "Font", &string) == 0 )
        {
            pango_font_description_free(self->text.font);
            self->text.font = pango_font_description_from_string(string);
        }
        else if ( self->parent != NULL )
        {
            pango_font_description_free(self->text.font);
            self->text.font = pango_font_description_copy_static(eventd_nd_style_get_text_font(self->parent));
        }

        if ( evhelpers_config_key_file_get_enum(config_file, "NotificationText", "Alignment", _eventd_nd_style_pango_alignments, G_N_ELEMENTS(_eventd_nd_style_pango_alignments), &enum_value) == 0 )
            self->text.align = enum_value;
        else if ( self->parent != NULL )
            self->text.align = eventd_nd_style_get_text_align(self->parent);

        if ( evhelpers_config_key_file_get_int(config_file, "NotificationText", "MaxLines", &integer) == 0 )
            self->text.max_lines = ( integer.value < 0 ) ? 0 : MAX(integer.value, 3);
        else if ( self->parent != NULL )
            self->text.max_lines = eventd_nd_style_get_text_max_lines(self->parent);

        if ( evhelpers_config_key_file_get_colour(config_file, "NotificationText", "Colour", &colour) == 0 )
            self->text.colour = colour;
        else if ( self->parent != NULL )
            self->text.colour = eventd_nd_style_get_text_colour(self->parent);
    }

    if ( g_key_file_has_group(config_file, "NotificationImage") )
    {
        self->image.set = TRUE;

        guint64 enum_value;
        Int integer;

        if ( evhelpers_config_key_file_get_enum(config_file, "NotificationImage", "Anchor", _eventd_nd_style_anchors_vertical, G_N_ELEMENTS(_eventd_nd_style_anchors_vertical), &enum_value) == 0 )
            self->image.anchor = enum_value;
        else if ( self->parent != NULL )
            self->image.anchor = eventd_nd_style_get_image_anchor(self->parent);

        if ( evhelpers_config_key_file_get_int(config_file, "NotificationImage", "MaxWidth", &integer) == 0 )
        {
            self->image.max_width = integer.value;
            *images_max_width  = MAX(*images_max_width, integer.value);
        }
        else if ( self->parent != NULL )
            self->image.max_width = eventd_nd_style_get_image_max_width(self->parent);

        if ( evhelpers_config_key_file_get_int(config_file, "NotificationImage", "MaxHeight", &integer) == 0 )
        {
            self->image.max_height = integer.value;
            *images_max_height  = MAX(*images_max_height, integer.value);
        }
        else if ( self->parent != NULL )
            self->image.max_height = eventd_nd_style_get_image_max_height(self->parent);

        if ( evhelpers_config_key_file_get_int(config_file, "NotificationImage", "Margin", &integer) == 0 )
            self->image.margin = integer.value;
        else if ( self->parent != NULL )
            self->image.margin = eventd_nd_style_get_image_margin(self->parent);

    }

    if ( g_key_file_has_group(config_file, "NotificationIcon") )
    {
        self->icon.set = TRUE;

        guint64 enum_value;
        Int integer;

        if ( evhelpers_config_key_file_get_enum(config_file, "NotificationIcon", "Placement", _eventd_nd_style_icon_placements, G_N_ELEMENTS(_eventd_nd_style_icon_placements), &enum_value) == 0 )
            self->icon.placement = enum_value;
        else if ( self->parent != NULL )
            self->icon.placement = eventd_nd_style_get_icon_placement(self->parent);

        if ( evhelpers_config_key_file_get_enum(config_file, "NotificationIcon", "Anchor", _eventd_nd_style_anchors_vertical, G_N_ELEMENTS(_eventd_nd_style_anchors_vertical), &enum_value) == 0 )
            self->icon.anchor = enum_value;
        else if ( self->parent != NULL )
            self->icon.anchor = eventd_nd_style_get_icon_anchor(self->parent);

        if ( evhelpers_config_key_file_get_int(config_file, "NotificationIcon", "MaxWidth", &integer) == 0 )
        {
            self->icon.max_width = integer.value;
            *images_max_width  = MAX(*images_max_width, integer.value);
        }
        else if ( self->parent != NULL )
            self->icon.max_width = eventd_nd_style_get_icon_max_width(self->parent);

        if ( evhelpers_config_key_file_get_int(config_file, "NotificationIcon", "MaxHeight", &integer) == 0 )
        {
            self->icon.max_height = integer.value;
            *images_max_height  = MAX(*images_max_height, integer.value);
        }
        else if ( self->parent != NULL )
            self->icon.max_height = eventd_nd_style_get_icon_max_height(self->parent);

        if ( evhelpers_config_key_file_get_int(config_file, "NotificationIcon", "Margin", &integer) == 0 )
            self->icon.margin = integer.value;
        else if ( self->parent != NULL )
            self->icon.margin = eventd_nd_style_get_icon_margin(self->parent);

        if ( evhelpers_config_key_file_get_int(config_file, "NotificationIcon", "FadeWidth", &integer) == 0 )
        {
            if ( ( integer.value < 0 ) || ( integer.value > 100 ) )
            {
                g_warning("Wrong percentage value '%" G_GINT64_FORMAT "'", integer.value);
                integer.value = CLAMP(integer.value, 0, 100);
            }
            self->icon.fade_width = (gdouble) integer.value / 100.;
        }
        else if ( self->parent != NULL )
            self->icon.fade_width = eventd_nd_style_get_icon_fade_width(self->parent);
    }
}

void
eventd_nd_style_free(gpointer data)
{
    EventdNdStyle *style = data;
    if ( style == NULL )
        return;

    pango_font_description_free(style->text.font);

    evhelpers_format_string_unref(style->template.text);
    evhelpers_filename_unref(style->template.image);
    evhelpers_filename_unref(style->template.icon);

    g_free(style);
}


FormatString *
eventd_nd_style_get_template_text(EventdNdStyle *self)
{
    if ( self->template.set )
        return self->template.text;
    return eventd_nd_style_get_template_text(self->parent);
}

Filename *
eventd_nd_style_get_template_image(EventdNdStyle *self)
{
    if ( self->template.set )
        return self->template.image;
    return eventd_nd_style_get_template_image(self->parent);
}

Filename *
eventd_nd_style_get_template_icon(EventdNdStyle *self)
{
    if ( self->template.set )
        return self->template.icon;
    return eventd_nd_style_get_template_icon(self->parent);
}

EventdNdAnchor
eventd_nd_style_get_bubble_anchor(EventdNdStyle *self)
{
    if ( self->bubble.set )
        return self->bubble.anchor;
    return eventd_nd_style_get_bubble_anchor(self->parent);
}

gint
eventd_nd_style_get_bubble_timeout(EventdNdStyle *self)
{
    if ( self->bubble.set )
        return self->bubble.timeout;
    return eventd_nd_style_get_bubble_timeout(self->parent);
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

const PangoFontDescription *
eventd_nd_style_get_text_font(EventdNdStyle *self)
{
    if ( self->text.set )
        return self->text.font;
    return eventd_nd_style_get_text_font(self->parent);
}

PangoAlignment
eventd_nd_style_get_text_align(EventdNdStyle *self)
{
    if ( self->text.set )
        return self->text.align;
    return eventd_nd_style_get_text_align(self->parent);
}

Colour
eventd_nd_style_get_text_colour(EventdNdStyle *self)
{
    if ( self->text.set )
        return self->text.colour;
    return eventd_nd_style_get_text_colour(self->parent);
}

guint8
eventd_nd_style_get_text_max_lines(EventdNdStyle *self)
{
    if ( self->text.set )
        return self->text.max_lines;
    return eventd_nd_style_get_text_max_lines(self->parent);
}

EventdNdAnchorVertical
eventd_nd_style_get_image_anchor(EventdNdStyle *self)
{
    if ( self->image.set )
        return self->image.anchor;
    return eventd_nd_style_get_image_anchor(self->parent);
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

EventdNdAnchorVertical
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
