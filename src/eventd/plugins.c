/*
 * eventd - Small daemon to act on remote or local events
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
#include <gmodule.h>

#include <eventd-plugin.h>
#include <plugins-helper.h>
#include "plugins.h"

static GList *plugins = NULL;

void
eventd_plugins_load()
{
    eventd_plugins_helper_load(&plugins, "plugins", NULL);
}

void
eventd_plugins_unload()
{
    eventd_plugins_helper_unload(&plugins);
}

void
eventd_plugins_config_init_all()
{
    eventd_plugins_helper_config_init_all(plugins);
}

void
eventd_plugins_config_clean_all()
{
    eventd_plugins_helper_config_clean_all(plugins);
}

void
eventd_plugins_event_parse_all(const gchar *type, const gchar *event, GKeyFile *config_file)
{
    eventd_plugins_helper_event_parse_all(plugins, type, event, config_file);
}

GHashTable *
eventd_plugins_event_action_all(EventdEvent *event)
{
    return eventd_plugins_helper_event_action_all(plugins, event);
}
