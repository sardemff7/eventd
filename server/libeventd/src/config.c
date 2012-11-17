/*
 * libeventd - Internal helper
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <glib.h>
#include <glib-object.h>

#include <libeventd-event.h>
#include <libeventd-regex.h>

#include <libeventd-config.h>

static gint8
_libeventd_config_key_file_error(GError **error, const gchar *group, const gchar *key)
{
    gint8 ret = 1;

    if ( *error == NULL )
        return 0;

    if ( (*error)->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND )
    {
        ret = -1;
        g_warning("Couldn't read the key [%s] '%s': %s", group, key, (*error)->message);
    }
    g_clear_error(error);

    return ret;
}

EVENTD_EXPORT
gint8
libeventd_config_key_file_get_boolean(GKeyFile *config_file, const gchar *group, const gchar *key, gboolean *value)
{
    GError *error = NULL;

    *value = g_key_file_get_boolean(config_file, group, key, &error);

    return _libeventd_config_key_file_error(&error, group, key);
}

EVENTD_EXPORT
gint8
libeventd_config_key_file_get_int(GKeyFile *config_file, const gchar *group, const gchar *key, Int *value)
{
    GError *error = NULL;

    value->value = g_key_file_get_int64(config_file, group, key, &error);
    value->set = ( error == NULL );

    return _libeventd_config_key_file_error(&error, group, key);
}

EVENTD_EXPORT
gint64
libeventd_config_key_file_get_int_with_default(GKeyFile *config_file, const gchar *group, const gchar *key, gint64 default_value)
{
    Int value;

    libeventd_config_key_file_get_int(config_file, group, key, &value);

    if ( value.set )
        return value.value;

    return default_value;
}

EVENTD_EXPORT
gint8
libeventd_config_key_file_get_string(GKeyFile *config_file, const gchar *group, const gchar *key, gchar **value)
{
    GError *error = NULL;

    *value = g_key_file_get_string(config_file, group, key, &error);

    return _libeventd_config_key_file_error(&error, group, key);
}

EVENTD_EXPORT
gint8
libeventd_config_key_file_get_string_list(GKeyFile *config_file, const gchar *group, const gchar *key, gchar ***value, gsize *length)
{
    GError *error = NULL;

    *value = g_key_file_get_string_list(config_file, group, key, length, &error);

    return _libeventd_config_key_file_error(&error, group, key);
}

static gboolean
_eventd_nd_style_parse_colour_value(const gchar *str, guint base, gdouble *r)
{
    gchar *next;
    guint64 v;
    v = g_ascii_strtoull(str, &next, base);
    *r = (gdouble) MIN(v, 255) / 255.;
    return ( str != next );
}

static gint
_eventd_nd_style_parse_colour(const gchar *sr, const gchar *sg, const gchar *sb, const gchar *sa, guint base, Colour *colour)
{
    gdouble r, g, b, a = 1.0;

    if ( ! _eventd_nd_style_parse_colour_value(sr, base, &r) )
        return 1;
    if ( ! _eventd_nd_style_parse_colour_value(sg, base, &g) )
        return 1;
    if ( ! _eventd_nd_style_parse_colour_value(sb, base, &b) )
        return 1;
    if ( ( sa != NULL ) && ( ! _eventd_nd_style_parse_colour_value(sa, base, &a) ) )
        return 1;

    colour->r = r;
    colour->g = g;
    colour->b = b;
    colour->a = a;

    return 0;
}

static gint
_eventd_nd_style_parse_rgba_colour(gchar *rgb, gboolean alpha, Colour *colour)
{
    const gchar *sr;
    const gchar *sg;
    const gchar *sb;
    const gchar *sa = NULL;

    sr = rgb;
    rgb = strchr(rgb, ',');
    *rgb = '\0';
    do ++rgb; while ( *rgb == ' ' );

    sg = rgb;
    rgb = strchr(rgb, ',');
    *rgb = '\0';
    do ++rgb; while ( *rgb == ' ' );

    sb = rgb;
    if ( alpha )
    {
        rgb = strchr(rgb, ',');
        *rgb = '\0';
        do ++rgb; while ( *rgb == ' ' );

        sa = rgb;
    }
    rgb = strchr(rgb, ')');
    *rgb = '\0';

    return _eventd_nd_style_parse_colour(sr, sg, sb, sa, 10, colour);
}

EVENTD_EXPORT
gint8
libeventd_config_key_file_get_colour(GKeyFile *config_file, const gchar *section, const gchar *key, Colour *colour)
{
    gchar *string;
    gint8 r;

    if ( ( r = libeventd_config_key_file_get_string(config_file, section, key, &string) ) != 0 )
        return r;

    r = 1;

    if ( g_str_has_prefix(string, "#") )
    {
        gchar *rgb = string;
        gchar hr[3] = {0};
        gchar hg[3] = {0};
        gchar hb[3] = {0};
        gchar ha[3] = {0};

        rgb += strlen("#");
        switch ( strlen(rgb) )
        {
        case 8: /* rrggbbaa */
            hr[0] = rgb[0]; hr[1] = rgb[1];
            hg[0] = rgb[2]; hg[1] = rgb[3];
            hb[0] = rgb[4]; hb[1] = rgb[5];
            ha[0] = rgb[6]; ha[1] = rgb[7];
        break;
        case 4: /* rgba */
            hr[0] = rgb[0];
            hg[0] = rgb[1];
            hb[0] = rgb[2];
            ha[0] = rgb[3];
        break;
        case 6: /* rrggbb */
            hr[0] = rgb[0]; hr[1] = rgb[1];
            hg[0] = rgb[2]; hg[1] = rgb[3];
            hb[0] = rgb[4]; hb[1] = rgb[5];
        break;
        case 3: /* rgb */
            hr[0] = rgb[0];
            hg[0] = rgb[1];
            hb[0] = rgb[2];
        break;
        }
        r = _eventd_nd_style_parse_colour(hr, hg, hb, ( ha[0] == '\0' ) ? NULL : ha, 16, colour);
    }
    else if ( g_str_has_prefix(string, "rgb(") && g_str_has_suffix(string, ")") )
        r = _eventd_nd_style_parse_rgba_colour(string + strlen("rgb("), FALSE, colour);
    else if ( g_str_has_prefix(string, "rgba(") && g_str_has_suffix(string, ")") )
        r = _eventd_nd_style_parse_rgba_colour(string + strlen("rgba("), TRUE, colour);

    g_free(string);

    return r;
}

EVENTD_EXPORT
gchar *
libeventd_config_get_filename(const gchar *filename, EventdEvent *event, const gchar *subdir)
{
    gchar *real_filename;

    if ( ! g_str_has_prefix(filename, "file://") )
    {
        filename = eventd_event_get_data(event, filename);
        if ( ( filename != NULL ) && g_str_has_prefix(filename, "file://") )
            real_filename = g_strdup(filename+7);
        else
            return NULL;
    }
    else
        real_filename = libeventd_regex_replace_event_data(filename+7, event, NULL, NULL);

    if ( ! g_path_is_absolute(real_filename) )
    {
        gchar *tmp;
        tmp = real_filename;
        real_filename = g_build_filename(g_get_user_data_dir(), PACKAGE_NAME, subdir, tmp, NULL);
        g_free(tmp);
    }

    if ( g_file_test(real_filename, G_FILE_TEST_IS_REGULAR) )
        return real_filename;

    g_free(real_filename);
    return g_strdup("");
}
