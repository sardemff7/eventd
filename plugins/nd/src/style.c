/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2017 Quentin "Sardem FF7" Glidic
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

#include "config.h"

#include <string.h>

#include <glib.h>
#include <pango/pango.h>

#include "libeventd-event.h"
#include "libeventd-helpers-config.h"

#include "style.h"

static const gchar * const _eventd_nd_style_pango_alignments[] = {
    [PANGO_ALIGN_LEFT]   = "left",
    [PANGO_ALIGN_RIGHT]  = "right",
    [PANGO_ALIGN_CENTER] = "center",
};

static const gchar * const _eventd_nd_style_pango_ellipsize_modes[] = {
    [PANGO_ELLIPSIZE_NONE]   = "none",
    [PANGO_ELLIPSIZE_START]  = "start",
    [PANGO_ELLIPSIZE_MIDDLE] = "middle",
    [PANGO_ELLIPSIZE_END]    = "end",
};

static const gchar * const _eventd_nd_style_anchors_vertical[] = {
    [EVENTD_ND_VANCHOR_TOP]     = "top",
    [EVENTD_ND_VANCHOR_BOTTOM]  = "bottom",
    [EVENTD_ND_VANCHOR_CENTER] = "center",
};

/* Not used
static const gchar * const _eventd_nd_style_anchors_horizontal[] = {
    [EVENTD_ND_HANCHOR_LEFT]    = "left",
    [EVENTD_ND_HANCHOR_RIGHT]   = "right",
    [EVENTD_ND_HANCHOR_CENTER] = "center",
};
*/

static const gchar * const _eventd_nd_style_icon_placements[] = {
    [EVENTD_ND_STYLE_ICON_PLACEMENT_BACKGROUND] = "background",
    [EVENTD_ND_STYLE_ICON_PLACEMENT_OVERLAY]    = "overlay",
    [EVENTD_ND_STYLE_ICON_PLACEMENT_FOREGROUND] = "foreground",
};

static const gchar * const _eventd_nd_style_progress_placements[] = {
    [EVENTD_ND_STYLE_PROGRESS_PLACEMENT_BAR_BOTTOM]       = "bar",
    [EVENTD_ND_STYLE_PROGRESS_PLACEMENT_IMAGE_BOTTOM_TOP] = "image bottom-to-top",
    [EVENTD_ND_STYLE_PROGRESS_PLACEMENT_IMAGE_TOP_BOTTOM] = "image top-to-bottom",
    [EVENTD_ND_STYLE_PROGRESS_PLACEMENT_IMAGE_LEFT_RIGHT] = "image left-to-right",
    [EVENTD_ND_STYLE_PROGRESS_PLACEMENT_IMAGE_RIGHT_LEFT] = "image right-to-left",
    [EVENTD_ND_STYLE_PROGRESS_PLACEMENT_IMAGE_CIRCULAR]   = "image circular",
};

struct _EventdPluginAction {
    EventdNdStyle *parent;

    struct {
        gboolean set;

        FormatString *text;
        Filename *image;
        Filename *icon;
        gchar *progress;
    } template;

    struct {
        gboolean set;

        gchar *queue;

        gint timeout;

        gint   min_width;
        gint   max_width;

        gint   padding;
        gint   radius;
        Colour colour;

        gint     border;
        Colour   border_colour;
        guint64  border_blur;
    } bubble;

    struct {
        gboolean set;

        PangoFontDescription *font;
        PangoAlignment align;
        EventdNdAnchorVertical valign;
        PangoEllipsizeMode ellipsize;
        guint8 max_lines;
        gint max_width;
        Colour colour;
    } text;

    struct {
        gboolean set;

        EventdNdAnchorVertical anchor;
        gint                   width;
        gint                   height;
        gboolean               fixed_size;
        gint                   margin;
    } image;

    struct {
        gboolean set;

        EventdNdStyleIconPlacement placement;
        EventdNdAnchorVertical     anchor;
        gint                       width;
        gint                       height;
        gint                       margin;
        gboolean                   fixed_size;
        gdouble                    fade_width;
    } icon;

    struct {
        gboolean set;

        EventdNdStyleProgressPlacement placement;
        gboolean                       reversed;
        gint64                         bar_width;
        Colour                         colour;
    } progress;
};

static void
_eventd_nd_style_init_defaults(EventdNdStyle *style)
{
    /* template */
    style->template.set     = TRUE;
    style->template.text   = evhelpers_format_string_new(g_strdup("<b>${title}</b>${message/^/\n}"));
    style->template.image   = evhelpers_filename_new(g_strdup("image"));
    style->template.icon    = evhelpers_filename_new(g_strdup("icon"));
    style->template.progress = g_strdup("progress-value");

    /* bubble */
    style->bubble.set = TRUE;

    /* bubble queue */
    style->bubble.queue  = g_strdup("default");

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

    /* border style */
    style->bubble.border          = 0;
    style->bubble.border_colour.r = 0.1;
    style->bubble.border_colour.g = 0.1;
    style->bubble.border_colour.b = 0.1;
    style->bubble.border_colour.a = 1.0;
    style->bubble.border_blur     = 5;

    /* text */
    style->text.set = TRUE;
    style->text.font        = pango_font_description_from_string("Linux Libertine O 9");
    style->text.align       = PANGO_ALIGN_LEFT;
    style->text.valign      = EVENTD_ND_VANCHOR_TOP;
    style->text.ellipsize   = PANGO_ELLIPSIZE_NONE;
    style->text.max_lines   = 10;
    style->text.max_width   = -1;
    style->text.colour.r    = 0.9;
    style->text.colour.g    = 0.9;
    style->text.colour.b    = 0.9;
    style->text.colour.a    = 1.0;

    /* image */
    style->image.set = TRUE;
    style->image.anchor     = EVENTD_ND_VANCHOR_TOP;
    style->image.width      = 50;
    style->image.height     = 50;
    style->image.fixed_size = FALSE;
    style->image.margin     = 10;

    /* icon */
    style->icon.set = TRUE;
    style->icon.placement  = EVENTD_ND_STYLE_ICON_PLACEMENT_BACKGROUND;
    style->icon.anchor     = EVENTD_ND_VANCHOR_CENTER;
    style->icon.width      = 25;
    style->icon.height     = 25;
    style->icon.margin     = 10;
    style->icon.fixed_size = FALSE;
    style->icon.fade_width = 0.75;

    /* progress */
    style->progress.set = TRUE;
    style->progress.placement = EVENTD_ND_STYLE_PROGRESS_PLACEMENT_BAR_BOTTOM;
    style->progress.reversed  = FALSE;
    style->progress.bar_width = 5;
    style->progress.colour.r  = 0.9;
    style->progress.colour.g  = 0.9;
    style->progress.colour.b  = 0.9;
    style->progress.colour.a  = 1.0;
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
eventd_nd_style_update(EventdNdStyle *self, GKeyFile *config_file)
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

        gchar *str = NULL;

        if ( evhelpers_config_key_file_get_string(config_file, "Notification", "Progress", &str) == 0 )
        {
            g_free(self->template.progress);
            self->template.progress = str;
        }
        else if ( self->parent != NULL )
        {
            g_free(self->template.progress);
            self->template.progress = g_strdup(eventd_nd_style_get_template_progress(self->parent));
        }
    }

    if ( g_key_file_has_group(config_file, "NotificationBubble") )
    {
        self->bubble.set = TRUE;

        gchar *string;
        Int integer;
        Colour colour;

        if ( evhelpers_config_key_file_get_string(config_file, "NotificationBubble", "Queue", &string) == 0 )
        {
            g_free(self->bubble.queue);
            self->bubble.queue = string;
        }
        else if ( self->parent != NULL )
        {
            g_free(self->bubble.queue);
            self->bubble.queue = g_strdup(eventd_nd_style_get_bubble_queue(self->parent));
        }

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

        if ( evhelpers_config_key_file_get_int(config_file, "NotificationBubble", "Border", &integer) == 0 )
            self->bubble.border = integer.value;
        else if ( self->parent != NULL )
            self->bubble.border = eventd_nd_style_get_bubble_border(self->parent);

        if ( evhelpers_config_key_file_get_colour(config_file, "NotificationBubble", "BorderColour", &colour) == 0 )
            self->bubble.border_colour = colour;
        else if ( self->parent != NULL )
            self->bubble.border_colour = eventd_nd_style_get_bubble_border_colour(self->parent);

        if ( evhelpers_config_key_file_get_int(config_file, "NotificationBubble", "BorderBlur", &integer) == 0 )
            self->bubble.border_blur = MAX(0, integer.value);
        else if ( self->parent != NULL )
            self->bubble.border_blur = eventd_nd_style_get_bubble_border_blur(self->parent);
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

        if ( evhelpers_config_key_file_get_enum(config_file, "NotificationText", "VerticalAlignment", _eventd_nd_style_anchors_vertical, G_N_ELEMENTS(_eventd_nd_style_anchors_vertical), &enum_value) == 0 )
            self->text.valign = enum_value;
        else if ( self->parent != NULL )
            self->text.valign = eventd_nd_style_get_text_valign(self->parent);

        if ( evhelpers_config_key_file_get_enum(config_file, "NotificationText", "Ellipsize", _eventd_nd_style_pango_ellipsize_modes, G_N_ELEMENTS(_eventd_nd_style_pango_ellipsize_modes), &enum_value) == 0 )
            self->text.ellipsize = enum_value;
        else if ( self->parent != NULL )
            self->text.ellipsize = eventd_nd_style_get_text_ellipsize(self->parent);

        if ( evhelpers_config_key_file_get_int(config_file, "NotificationText", "MaxLines", &integer) == 0 )
            self->text.max_lines = ( integer.value < 0 ) ? 0 : MAX(integer.value, 3);
        else if ( self->parent != NULL )
            self->text.max_lines = eventd_nd_style_get_text_max_lines(self->parent);

        if ( evhelpers_config_key_file_get_int(config_file, "NotificationText", "MaxWidth", &integer) == 0 )
            self->text.max_width = integer.value;
        else if ( self->parent != NULL )
            self->text.max_width = eventd_nd_style_get_text_max_width(self->parent);

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
        gboolean boolean;

        if ( evhelpers_config_key_file_get_enum(config_file, "NotificationImage", "Anchor", _eventd_nd_style_anchors_vertical, G_N_ELEMENTS(_eventd_nd_style_anchors_vertical), &enum_value) == 0 )
            self->image.anchor = enum_value;
        else if ( self->parent != NULL )
            self->image.anchor = eventd_nd_style_get_image_anchor(self->parent);

        if ( evhelpers_config_key_file_get_int(config_file, "NotificationImage", "Width", &integer) == 0 )
            self->image.width = integer.value;
        else if ( self->parent != NULL )
            self->image.width = eventd_nd_style_get_image_width(self->parent);

        if ( evhelpers_config_key_file_get_int(config_file, "NotificationImage", "Height", &integer) == 0 )
            self->image.height = integer.value;
        else if ( self->parent != NULL )
            self->image.height = eventd_nd_style_get_image_height(self->parent);

        if ( evhelpers_config_key_file_get_boolean(config_file, "NotificationImage", "FixedSize", &boolean) == 0 )
            self->image.fixed_size = boolean;
        else if ( self->parent != NULL )
            self->image.fixed_size = eventd_nd_style_get_image_fixed_size(self->parent);

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
        gboolean boolean;

        if ( evhelpers_config_key_file_get_enum(config_file, "NotificationIcon", "Placement", _eventd_nd_style_icon_placements, G_N_ELEMENTS(_eventd_nd_style_icon_placements), &enum_value) == 0 )
            self->icon.placement = enum_value;
        else if ( self->parent != NULL )
            self->icon.placement = eventd_nd_style_get_icon_placement(self->parent);

        if ( evhelpers_config_key_file_get_enum(config_file, "NotificationIcon", "Anchor", _eventd_nd_style_anchors_vertical, G_N_ELEMENTS(_eventd_nd_style_anchors_vertical), &enum_value) == 0 )
            self->icon.anchor = enum_value;
        else if ( self->parent != NULL )
            self->icon.anchor = eventd_nd_style_get_icon_anchor(self->parent);

        if ( evhelpers_config_key_file_get_int(config_file, "NotificationIcon", "Width", &integer) == 0 )
            self->icon.width = integer.value;
        else if ( self->parent != NULL )
            self->icon.width = eventd_nd_style_get_icon_width(self->parent);

        if ( evhelpers_config_key_file_get_int(config_file, "NotificationIcon", "Height", &integer) == 0 )
            self->icon.height = integer.value;
        else if ( self->parent != NULL )
            self->icon.height = eventd_nd_style_get_icon_height(self->parent);

        if ( evhelpers_config_key_file_get_boolean(config_file, "NotificationIcon", "FixedSize", &boolean) == 0 )
            self->icon.fixed_size = boolean;
        else if ( self->parent != NULL )
            self->icon.fixed_size = eventd_nd_style_get_icon_fixed_size(self->parent);

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

    if ( g_key_file_has_group(config_file, "NotificationProgress") )
    {
        self->progress.set = TRUE;

        guint64 enum_value;
        gboolean boolean;
        Int integer;
        Colour colour;

        if ( evhelpers_config_key_file_get_enum(config_file, "NotificationProgress", "Placement", _eventd_nd_style_progress_placements, G_N_ELEMENTS(_eventd_nd_style_progress_placements), &enum_value) == 0 )
            self->progress.placement = enum_value;
        else if ( self->parent != NULL )
            self->progress.placement = eventd_nd_style_get_progress_placement(self->parent);

        if ( evhelpers_config_key_file_get_boolean(config_file, "NotificationProgress", "Reversed", &boolean) == 0 )
            self->progress.reversed = boolean;
        else if ( self->parent != NULL )
            self->progress.reversed = eventd_nd_style_get_progress_reversed(self->parent);

        if ( evhelpers_config_key_file_get_int(config_file, "NotificationProgress", "BarWidth", &integer) == 0 )
            self->progress.bar_width = integer.value;
        else if ( self->parent != NULL )
            self->progress.bar_width = eventd_nd_style_get_progress_bar_width(self->parent);

        if ( evhelpers_config_key_file_get_colour(config_file, "NotificationProgress", "Colour", &colour) == 0 )
            self->progress.colour = colour;
        else if ( self->parent != NULL )
            self->progress.colour = eventd_nd_style_get_progress_colour(self->parent);
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

const gchar *
eventd_nd_style_get_template_progress(EventdNdStyle *self)
{
    if ( self->template.set )
        return self->template.progress;
    return eventd_nd_style_get_template_progress(self->parent);
}

const gchar *
eventd_nd_style_get_bubble_queue(EventdNdStyle *self)
{
    if ( self->bubble.set )
        return self->bubble.queue;
    return eventd_nd_style_get_bubble_queue(self->parent);
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

gint
eventd_nd_style_get_bubble_border(EventdNdStyle *self)
{
    if ( self->bubble.set )
        return self->bubble.border;
    return eventd_nd_style_get_bubble_border(self->parent);
}

Colour
eventd_nd_style_get_bubble_border_colour(EventdNdStyle *self)
{
    if ( self->bubble.set )
        return self->bubble.border_colour;
    return eventd_nd_style_get_bubble_border_colour(self->parent);
}

guint64
eventd_nd_style_get_bubble_border_blur(EventdNdStyle *self)
{
    if ( self->bubble.set )
        return self->bubble.border_blur;
    return eventd_nd_style_get_bubble_border_blur(self->parent);
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

EventdNdAnchorVertical
eventd_nd_style_get_text_valign(EventdNdStyle *self)
{
    if ( self->text.set )
        return self->text.valign;
    return eventd_nd_style_get_text_valign(self->parent);
}

Colour
eventd_nd_style_get_text_colour(EventdNdStyle *self)
{
    if ( self->text.set )
        return self->text.colour;
    return eventd_nd_style_get_text_colour(self->parent);
}

PangoEllipsizeMode
eventd_nd_style_get_text_ellipsize(EventdNdStyle *self)
{
    if ( self->text.set )
        return self->text.ellipsize;
    return eventd_nd_style_get_text_ellipsize(self->parent);
}

guint8
eventd_nd_style_get_text_max_lines(EventdNdStyle *self)
{
    if ( self->text.set )
        return self->text.max_lines;
    return eventd_nd_style_get_text_max_lines(self->parent);
}

gint
eventd_nd_style_get_text_max_width(EventdNdStyle *self)
{
    if ( self->text.set )
        return self->text.max_width;
    return eventd_nd_style_get_text_max_width(self->parent);
}

EventdNdAnchorVertical
eventd_nd_style_get_image_anchor(EventdNdStyle *self)
{
    if ( self->image.set )
        return self->image.anchor;
    return eventd_nd_style_get_image_anchor(self->parent);
}

gint
eventd_nd_style_get_image_width(EventdNdStyle *self)
{
    if ( self->image.set )
        return self->image.width;
    return eventd_nd_style_get_image_width(self->parent);
}

gint
eventd_nd_style_get_image_height(EventdNdStyle *self)
{
    if ( self->image.set )
        return self->image.height;
    return eventd_nd_style_get_image_height(self->parent);
}

void
eventd_nd_style_get_image_size(EventdNdStyle *self, gint max_draw_width, gint *width, gint *height)
{
    *width = eventd_nd_style_get_image_width(self);
    *height = eventd_nd_style_get_image_height(self);

    if ( ( *width < 0 ) || ( *width > max_draw_width ) )
        *width = max_draw_width;
}

gboolean
eventd_nd_style_get_image_fixed_size(EventdNdStyle *self)
{
    if ( self->image.set )
        return self->image.fixed_size;
    return eventd_nd_style_get_image_fixed_size(self->parent);
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
eventd_nd_style_get_icon_width(EventdNdStyle *self)
{
    if ( self->icon.set )
        return self->icon.width;
    return eventd_nd_style_get_icon_width(self->parent);
}

gint
eventd_nd_style_get_icon_height(EventdNdStyle *self)
{
    if ( self->icon.set )
        return self->icon.height;
    return eventd_nd_style_get_icon_height(self->parent);
}

void
eventd_nd_style_get_icon_size(EventdNdStyle *self, gint max_draw_width, gint *width, gint *height)
{
    *width = eventd_nd_style_get_icon_width(self);
    *height = eventd_nd_style_get_icon_height(self);

    if ( ( *width < 0 ) || ( *width > max_draw_width ) )
        *width = max_draw_width;
}

gboolean
eventd_nd_style_get_icon_fixed_size(EventdNdStyle *self)
{
    if ( self->icon.set )
        return self->icon.fixed_size;
    return eventd_nd_style_get_icon_fixed_size(self->parent);
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


EventdNdStyleProgressPlacement
eventd_nd_style_get_progress_placement(EventdNdStyle *self)
{
    if ( self->progress.set )
        return self->progress.placement;
    return eventd_nd_style_get_progress_placement(self->parent);
}

gboolean
eventd_nd_style_get_progress_reversed(EventdNdStyle *self)
{
    if ( self->progress.set )
        return self->progress.reversed;
    return eventd_nd_style_get_progress_reversed(self->parent);
}

gint
eventd_nd_style_get_progress_bar_width(EventdNdStyle *self)
{
    if ( self->progress.set )
        return self->progress.bar_width;
    return eventd_nd_style_get_progress_bar_width(self->parent);
}

Colour
eventd_nd_style_get_progress_colour(EventdNdStyle *self)
{
    if ( self->progress.set )
        return self->progress.colour;
    return eventd_nd_style_get_progress_colour(self->parent);
}
