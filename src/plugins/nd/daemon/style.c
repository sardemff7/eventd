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

#include <cairo.h>
#include <pango/pango.h>

#include <libeventd-config.h>

#include "../types.h"

#include "style.h"
#include "style-internal.h"

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

    /* icon */
    style->icon_max_width  = 50;
    style->icon_max_height = 50;
    style->icon_spacing    = 10;
    style->overlay_scale   = 0.5;

    /* fonts */
    style->pango_context = pango_context_new();

    /* title */
    style->title_font_string = g_strdup("Linux Libertine O Bold 9");
    style->title_colour.r    = 0.9;
    style->title_colour.g    = 0.9;
    style->title_colour.b    = 0.9;
    style->title_colour.a    = 1.0;

    /* message */
    style->message_spacing     = 5;
    style->message_max_lines   = 10;
    style->message_font_string = g_strdup("Linux Libertine O 9");
    style->message_colour.r    = 0.9;
    style->message_colour.g    = 0.9;
    style->message_colour.b    = 0.9;
    style->message_colour.a    = 1.0;
}

static void
_eventd_nd_style_load_dir(EventdNdStyle *style, const gchar *base_dir)
{
    GError *error = NULL;
    gchar *config_file_name;
    GKeyFile *config_file;
    gchar *string;
    Int integer;
    Colour colour;

    config_file_name = g_build_filename(base_dir, PACKAGE_NAME, "notification-daemon.conf", NULL);
    if ( ! g_file_test(config_file_name, G_FILE_TEST_IS_REGULAR) )
    {
        g_free(config_file_name);
        return;
    }

    config_file = g_key_file_new();
    if ( ! g_key_file_load_from_file(config_file, config_file_name, G_KEY_FILE_NONE, &error) )
    {
        g_warning("Can't read the configuration file '%s': %s", config_file_name, error->message);
        goto fail;
    }

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
            g_free(style->title_font_string);
            style->title_font_string = string;
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
            g_free(style->message_font_string);
            style->message_font_string = string;
        }

        if ( ( libeventd_config_key_file_get_string(config_file, "message", "colour", &string) == 0 ) && _eventd_nd_style_parse_colour(string, &colour) )
        {
            g_free(string);
            style->message_colour = colour;
        }
    }

    if ( g_key_file_has_group(config_file, "icon") )
    {
        if ( libeventd_config_key_file_get_int(config_file, "icon", "max-width", &integer) == 0 )
            style->icon_max_width = integer.value;

        if ( libeventd_config_key_file_get_int(config_file, "icon", "max-height", &integer) == 0 )
            style->icon_max_height = integer.value;

        if ( libeventd_config_key_file_get_int(config_file, "icon", "overlay-scale", &integer) == 0 )
            style->overlay_scale = (gdouble)integer.value / 100.;

        if ( libeventd_config_key_file_get_int(config_file, "icon", "spacing", &integer) == 0 )
            style->icon_spacing = integer.value;
    }


fail:
    g_key_file_free(config_file);
    g_clear_error(&error);
}

static void
_eventd_nd_style_process_fonts(EventdNdStyle *style)
{
    style->title_font = pango_font_description_from_string(style->title_font_string);
    style->message_font = pango_font_description_from_string(style->message_font_string);
}


EventdNdStyle *
eventd_nd_style_new()
{
    EventdNdStyle *style;

    style = g_new0(EventdNdStyle, 1);

    _eventd_nd_style_init_defaults(style);

    _eventd_nd_style_load_dir(style, DATADIR);
    _eventd_nd_style_load_dir(style, SYSCONFDIR);
    _eventd_nd_style_load_dir(style, g_get_user_config_dir());

    _eventd_nd_style_process_fonts(style);

    return style;
}

void
eventd_nd_style_free(EventdNdStyle *style)
{
    if ( style == NULL )
        return;

    pango_font_description_free(style->message_font);
    pango_font_description_free(style->title_font);

    g_free(style->title_font_string);
    g_free(style->message_font_string);

    g_object_unref(style->pango_context);

    g_free(style);
}
