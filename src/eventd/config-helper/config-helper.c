/*
 * libeventd-config-helper - Internal onfig helper
 *
 * Copyright © 2011 Quentin "Sardem FF7" Glidic
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
#include <gio/gio.h>

#include <config-helper.h>

gint8
eventd_config_key_file_get_string(GKeyFile *config_file, const gchar *group, const gchar *key, const gchar *event, const gchar *type, gchar **value)
{
    GError *error = NULL;
    gint8 ret = 0;
    gchar *ret_value = NULL;

    ret_value = g_key_file_get_string(config_file, group, key, &error);
    if ( ( ! ret_value ) && ( error->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND ) )
    {
        ret = -1;
        g_warning("Can't set the %s action of event '%s' for client type '%s': %s", group, event, type, error->message);
    }
    else if ( ! ret_value )
    {
        ret = 1;
        *value = NULL;
    }
    else
        *value = ret_value;
    g_clear_error(&error);

    return ret;
}
