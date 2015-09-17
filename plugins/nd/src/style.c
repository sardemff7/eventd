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

#include <config.h>

#include <glib.h>
#include <pango/pango.h>

#ifdef ENABLE_GDK_PIXBUF
#include <gdk-pixbuf/gdk-pixbuf.h>
#endif /* ENABLE_GDK_PIXBUF */

#include <libeventd-event.h>
#include <libeventd-helpers-config.h>

#include "style.h"

static const gchar * const _eventd_nd_style_pango_alignments[] = {
    [PANGO_ALIGN_LEFT]   = "left",
    [PANGO_ALIGN_RIGHT]  = "right",
    [PANGO_ALIGN_CENTER] = "center",
};

static const gchar * const _eventd_nd_style_anchors_vertical[] = {
    [EVENTD_ND_ANCHOR_TOP]     = "top",
    [EVENTD_ND_ANCHOR_BOTTOM]  = "bottom",
    [EVENTD_ND_ANCHOR_VCENTER] = "center",
};

static const gchar * const _eventd_nd_style_anchors_horizontal[] = {
    [EVENTD_ND_ANCHOR_LEFT]    = "left",
    [EVENTD_ND_ANCHOR_RIGHT]   = "right",
    [EVENTD_ND_ANCHOR_HCENTER] = "center",
};

static const gchar * const _eventd_nd_style_icon_placements[] = {
    [EVENTD_ND_STYLE_ICON_PLACEMENT_BACKGROUND] = "background",
    [EVENTD_ND_STYLE_ICON_PLACEMENT_OVERLAY]    = "overlay",
    [EVENTD_ND_STYLE_ICON_PLACEMENT_FOREGROUND] = "foreground",
};

struct _EventdNdStyle {
    EventdNdStyle *parent;

    struct {
        gboolean set;

        FormatString *title;
        FormatString *message;
#ifdef ENABLE_GDK_PIXBUF
        Filename *image;
        Filename *icon;
#endif /* ENABLE_GDK_PIXBUF */
    } template;

    struct {
        gboolean set;

        gint   min_width;
        gint   max_width;

        gint   padding;
        gint   radius;
        Colour colour;
    } bubble;

#ifdef ENABLE_GDK_PIXBUF
    struct {
        gboolean set;

        gint max_width;
        gint max_height;
        gint margin;
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
#endif /* ENABLE_GDK_PIXBUF */

    struct {
        gboolean set;

        PangoFontDescription *font;
        PangoAlignment align;
        Colour colour;
    } title;

    struct {
        gboolean set;

        gint   spacing;
        guint8 max_lines;
        PangoFontDescription *font;
        PangoAlignment align;
        Colour colour;
    } message;
};

struct _EventdNdNotificationContents {
    gchar *title;
    gchar *message;
#ifdef ENABLE_GDK_PIXBUF
    GdkPixbuf *image;
    GdkPixbuf *icon;
#endif /* ENABLE_GDK_PIXBUF */
};

static void
_eventd_nd_style_init_defaults(EventdNdStyle *style)
{
    /* template */
    style->template.set     = TRUE;
    style->template.title   = evhelpers_format_string_new(g_strdup("${name}"));
    style->template.message = evhelpers_format_string_new(g_strdup("${text}"));
#ifdef ENABLE_GDK_PIXBUF
    style->template.image   = evhelpers_filename_new(g_strdup("image"));
    style->template.icon    = evhelpers_filename_new(g_strdup("icon"));
#endif /* ENABLE_GDK_PIXBUF */

    /* bubble */
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

    /* title */
    style->title.set = TRUE;
    style->title.font        = pango_font_description_from_string("Linux Libertine O Bold 9");
    style->title.align       = PANGO_ALIGN_LEFT;
    style->title.colour.r    = 0.9;
    style->title.colour.g    = 0.9;
    style->title.colour.b    = 0.9;
    style->title.colour.a    = 1.0;

    /* message */
    style->message.set = TRUE;
    style->message.spacing     = 5;
    style->message.max_lines   = 10;
    style->message.font        = pango_font_description_from_string("Linux Libertine O 9");
    style->message.align       = PANGO_ALIGN_LEFT;
    style->message.colour.r    = 0.9;
    style->message.colour.g    = 0.9;
    style->message.colour.b    = 0.9;
    style->message.colour.a    = 1.0;

#ifdef ENABLE_GDK_PIXBUF
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
#endif /* ENABLE_GDK_PIXBUF */
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

        evhelpers_format_string_unref(self->template.title);
        if ( evhelpers_config_key_file_get_locale_format_string(config_file, "Notification", "Title", NULL, &string) == 0 )
            self->template.title = string;
        else if ( self->parent != NULL )
            self->template.title = evhelpers_format_string_ref(eventd_nd_style_get_template_title(self->parent));

        string = NULL;
        evhelpers_format_string_unref(self->template.message);
        if ( evhelpers_config_key_file_get_locale_format_string(config_file, "Notification", "Message", NULL, &string) == 0 )
            self->template.message = string;
        else if ( self->parent != NULL )
            self->template.message = evhelpers_format_string_ref(eventd_nd_style_get_template_message(self->parent));

#ifdef ENABLE_GDK_PIXBUF
        Filename *filename = NULL;

        evhelpers_filename_unref(self->template.image);
        if ( evhelpers_config_key_file_get_filename(config_file, "Notification", "Image", &filename) == 0 )
            self->template.image = filename;
        else if ( self->parent != NULL )
            self->template.image = evhelpers_filename_ref(eventd_nd_style_get_template_image(self->parent));

        filename = NULL;
        evhelpers_filename_unref(self->template.icon);
        if ( evhelpers_config_key_file_get_filename(config_file, "Notification", "Icon", &filename) == 0 )
            self->template.icon = filename;
        else if ( self->parent != NULL )
            self->template.icon = evhelpers_filename_ref(eventd_nd_style_get_template_icon(self->parent));
#endif /* ENABLE_GDK_PIXBUF */
    }

    if ( g_key_file_has_group(config_file, "NotificationBubble") )
    {
        self->bubble.set = TRUE;

        Int integer;
        Colour colour;

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

        /* We ignore the minimum width if larger than the maximum */
        if ( ( self->bubble.max_width > -1 ) && ( self->bubble.min_width > self->bubble.max_width ) )
            self->bubble.min_width = 0;
    }

    if ( g_key_file_has_group(config_file, "NotificationTitle") )
    {
        self->title.set = TRUE;

        gchar *string;
        guint64 enum_value;
        Colour colour;

        if ( evhelpers_config_key_file_get_string(config_file, "NotificationTitle", "Font", &string) == 0 )
        {
            pango_font_description_free(self->title.font);
            self->title.font = pango_font_description_from_string(string);
        }
        else if ( self->parent != NULL )
        {
            pango_font_description_free(self->title.font);
            self->title.font = pango_font_description_copy_static(eventd_nd_style_get_title_font(self->parent));
        }

        if ( evhelpers_config_key_file_get_enum(config_file, "NotificationTitle", "Alignment", _eventd_nd_style_pango_alignments, G_N_ELEMENTS(_eventd_nd_style_pango_alignments), &enum_value) == 0 )
            self->title.align = enum_value;
        else if ( self->parent != NULL )
            self->title.align = eventd_nd_style_get_title_align(self->parent);

        if ( evhelpers_config_key_file_get_colour(config_file, "NotificationTitle", "Colour", &colour) == 0 )
            self->title.colour = colour;
        else if ( self->parent != NULL )
            self->title.colour = eventd_nd_style_get_title_colour(self->parent);

    }

    if ( g_key_file_has_group(config_file, "NotificationMessage") )
    {
        self->message.set = TRUE;

        Int integer;
        gchar *string;
        guint64 enum_value;
        Colour colour;

        if ( evhelpers_config_key_file_get_int(config_file, "NotificationMessage", "Spacing", &integer) == 0 )
            self->message.spacing = integer.value;
        else if ( self->parent != NULL )
            self->message.spacing = eventd_nd_style_get_message_spacing(self->parent);

        if ( evhelpers_config_key_file_get_int(config_file, "NotificationMessage", "MaxLines", &integer) == 0 )
            self->message.max_lines = integer.value;
        else if ( self->parent != NULL )
            self->message.max_lines = eventd_nd_style_get_message_max_lines(self->parent);

        if ( evhelpers_config_key_file_get_string(config_file, "NotificationMessage", "Font", &string) == 0 )
        {
            pango_font_description_free(self->message.font);
            self->message.font = pango_font_description_from_string(string);
        }
        else if ( self->parent != NULL )
        {
            pango_font_description_free(self->message.font);
            self->message.font = pango_font_description_copy_static(eventd_nd_style_get_message_font(self->parent));
        }

        if ( evhelpers_config_key_file_get_enum(config_file, "NotificationMessage", "Alignment", _eventd_nd_style_pango_alignments, G_N_ELEMENTS(_eventd_nd_style_pango_alignments), &enum_value) == 0 )
            self->message.align = enum_value;
        else if ( self->parent != NULL )
            self->message.align = eventd_nd_style_get_message_align(self->parent);

        if ( evhelpers_config_key_file_get_colour(config_file, "NotificationMessage", "Colour", &colour) == 0 )
            self->message.colour = colour;
        else if ( self->parent != NULL )
            self->message.colour = eventd_nd_style_get_message_colour(self->parent);

    }

#ifdef ENABLE_GDK_PIXBUF
    if ( g_key_file_has_group(config_file, "NotificationImage") )
    {
        self->image.set = TRUE;

        Int integer;

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

        if ( evhelpers_config_key_file_get_enum(config_file, "NotificationIcon", "Anche", _eventd_nd_style_anchors_vertical, G_N_ELEMENTS(_eventd_nd_style_anchors_vertical), &enum_value) == 0 )
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
                g_warning("Wrong percentage value '%jd'", integer.value);
                integer.value = CLAMP(integer.value, 0, 100);
            }
            self->icon.fade_width = (gdouble) integer.value / 100.;
        }
        else if ( self->parent != NULL )
            self->icon.fade_width = eventd_nd_style_get_icon_fade_width(self->parent);
    }
#endif /* ENABLE_GDK_PIXBUF */
}

void
eventd_nd_style_free(gpointer data)
{
    EventdNdStyle *style = data;
    if ( style == NULL )
        return;

    pango_font_description_free(style->title.font);
    pango_font_description_free(style->message.font);

    evhelpers_format_string_unref(style->template.title);
    evhelpers_format_string_unref(style->template.message);
#ifdef ENABLE_GDK_PIXBUF
    evhelpers_filename_unref(style->template.image);
    evhelpers_filename_unref(style->template.icon);
#endif /* ENABLE_GDK_PIXBUF */

    g_free(style);
}


FormatString *
eventd_nd_style_get_template_title(EventdNdStyle *self)
{
    if ( self->template.set )
        return self->template.title;
    return eventd_nd_style_get_template_title(self->parent);
}

FormatString *
eventd_nd_style_get_template_message(EventdNdStyle *self)
{
    if ( self->template.set )
        return self->template.message;
    return eventd_nd_style_get_template_message(self->parent);
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
eventd_nd_style_get_title_font(EventdNdStyle *self)
{
    if ( self->title.set )
        return self->title.font;
    return eventd_nd_style_get_title_font(self->parent);
}

PangoAlignment
eventd_nd_style_get_title_align(EventdNdStyle *self)
{
    if ( self->title.set )
        return self->title.align;
    return eventd_nd_style_get_title_align(self->parent);
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

PangoAlignment
eventd_nd_style_get_message_align(EventdNdStyle *self)
{
    if ( self->message.set )
        return self->message.align;
    return eventd_nd_style_get_message_align(self->parent);
}

Colour
eventd_nd_style_get_message_colour(EventdNdStyle *self)
{
    if ( self->message.set )
        return self->message.colour;
    return eventd_nd_style_get_message_colour(self->parent);
}

#ifdef ENABLE_GDK_PIXBUF
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
#endif /* ENABLE_GDK_PIXBUF */


/* EventdNdNotificationContents */


#ifdef ENABLE_GDK_PIXBUF
static GdkPixbuf *
_eventd_nd_notification_contents_pixbuf_from_file(const gchar *path, gint width, gint height)
{
    GError *error = NULL;
    GdkPixbufFormat *format;
    GdkPixbuf *pixbuf;

    if ( *path == 0 )
        return NULL;

    if ( ( ( width > 0 ) || ( height > 0 ) ) && ( ( format = gdk_pixbuf_get_file_info(path, NULL, NULL) ) != NULL ) && gdk_pixbuf_format_is_scalable(format) )
        pixbuf = gdk_pixbuf_new_from_file_at_size(path, width, height, &error);
    else
        pixbuf = gdk_pixbuf_new_from_file(path, &error);

    if ( pixbuf == NULL )
        g_warning("Couldn't load file '%s': %s", path, error->message);
    g_clear_error(&error);

    return pixbuf;
}

static void
_eventd_nd_notification_contents_pixbuf_data_free(guchar *pixels, gpointer data)
{
    g_free(pixels);
}

static GdkPixbuf *
_eventd_nd_notification_contents_pixbuf_from_base64(EventdEvent *event, const gchar *name)
{
    GdkPixbuf *pixbuf = NULL;
    const gchar *base64;
    guchar *data;
    gsize length;
    const gchar *format;
    gchar *format_name;

    base64 = eventd_event_get_data(event, name);
    if ( base64 == NULL )
        return NULL;
    data = g_base64_decode(base64, &length);

    format_name = g_strconcat(name, "-format", NULL);
    format = eventd_event_get_data(event, format_name);
    g_free(format_name);

    if ( format != NULL )
    {
        gint width, height;
        gint stride;
        gboolean alpha;
        gchar *f;

        width = g_ascii_strtoll(format, &f, 16);
        height = g_ascii_strtoll(f+1, &f, 16);
        stride = g_ascii_strtoll(f+1, &f, 16);
        alpha = g_ascii_strtoll(f+1, &f, 16);

        pixbuf = gdk_pixbuf_new_from_data(data, GDK_COLORSPACE_RGB, alpha, 8, width, height, stride, _eventd_nd_notification_contents_pixbuf_data_free, NULL);
    }
    else
    {
        GError *error = NULL;
        GdkPixbufLoader *loader;

        loader = gdk_pixbuf_loader_new();

        if ( ! gdk_pixbuf_loader_write(loader, data, length, &error) )
        {
            g_warning("Couldn't write image data: %s", error->message);
            g_clear_error(&error);
            goto error;
        }

        if ( ! gdk_pixbuf_loader_close(loader, &error) )
        {
            g_warning("Couldn't load image data: %s", error->message);
            g_clear_error(&error);
            goto error;
        }

        pixbuf = g_object_ref(gdk_pixbuf_loader_get_pixbuf(loader));

    error:
        g_object_unref(loader);
        g_free(data);
    }

    return pixbuf;
}
#endif /* ENABLE_GDK_PIXBUF */

EventdNdNotificationContents *
eventd_nd_notification_contents_new(EventdNdStyle *style, EventdEvent *event, gint width, gint height)
{
    EventdNdNotificationContents *self;

    const FormatString *title = eventd_nd_style_get_template_title(style);
    const FormatString *message = eventd_nd_style_get_template_message(style);

    self = g_new0(EventdNdNotificationContents, 1);

    self->title = evhelpers_format_string_get_string(title, event, NULL, NULL);

    self->message = evhelpers_format_string_get_string(message, event, NULL, NULL);
    if ( *self->message == '\0' )
        /* Empty message, just skip it */
        self->message = (g_free(self->message), NULL);

#ifdef ENABLE_GDK_PIXBUF
    const Filename *image = eventd_nd_style_get_template_image(style);
    const Filename *icon = eventd_nd_style_get_template_icon(style);
    gchar *path;
    const gchar *data;

    if ( evhelpers_filename_get_path(image, event, "images", &data, &path) )
    {
        if ( path != NULL )
            self->image = _eventd_nd_notification_contents_pixbuf_from_file(path, width, height);
        else if ( data != NULL )
           self->image =  _eventd_nd_notification_contents_pixbuf_from_base64(event, data);
        g_free(path);
    }

    if ( evhelpers_filename_get_path(icon, event, "icons", &data, &path) )
    {
        if ( path != NULL )
            self->icon = _eventd_nd_notification_contents_pixbuf_from_file(path, width, height);
        else if ( data != NULL )
            self->icon = _eventd_nd_notification_contents_pixbuf_from_base64(event, data);
        g_free(path);
    }
#endif /* ENABLE_GDK_PIXBUF */

    return self;
}

void
eventd_nd_notification_contents_free(EventdNdNotificationContents *self)
{
    if ( self == NULL )
        return;

#ifdef ENABLE_GDK_PIXBUF
    if ( self->icon != NULL )
        g_object_unref(self->icon);
    if ( self->image != NULL )
        g_object_unref(self->image);
#endif /* ENABLE_GDK_PIXBUF */
    g_free(self->message);
    g_free(self->title);

    g_free(self);
}

const gchar *
eventd_nd_notification_contents_get_title(EventdNdNotificationContents *self)
{
    return self->title;
}

const gchar *
eventd_nd_notification_contents_get_message(EventdNdNotificationContents *self)
{
    return self->message;
}

#ifdef ENABLE_GDK_PIXBUF
GdkPixbuf *
eventd_nd_notification_contents_get_image(EventdNdNotificationContents *self)
{
    return self->image;
}

GdkPixbuf *
eventd_nd_notification_contents_get_icon(EventdNdNotificationContents *self)
{
    return self->icon;
}
#endif /* ENABLE_GDK_PIXBUF */

