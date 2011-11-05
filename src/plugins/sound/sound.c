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

#include <eventd-plugin.h>
#include <plugins-helper.h>

#include "pulseaudio.h"

static GList *plugins = NULL;

static void
eventd_sound_start(gpointer user_data)
{
    EventdSoundPulseaudioContext *context;

    context = eventd_sound_pulseaudio_start();
    eventd_plugins_helper_load(&plugins, "plugins" G_DIR_SEPARATOR_S "sound", context);

    g_free(context);
}

static void
eventd_sound_stop()
{
    eventd_plugins_helper_unload(&plugins);
    eventd_sound_pulseaudio_stop();
}

static GHashTable *
eventd_sound_event_action(EventdEvent *event)
{
    return eventd_plugins_helper_event_action_all(plugins, event);
}

static void
eventd_sound_event_parse(const gchar *type, const gchar *event, GKeyFile *config_file)
{
    eventd_plugins_helper_event_parse_all(plugins, type, event, config_file);
}

static void
eventd_sound_config_init()
{
    eventd_plugins_helper_config_init_all(plugins);
}

static void
eventd_sound_config_clean()
{
    eventd_plugins_helper_config_clean_all(plugins);
}

void
eventd_plugin_get_info(EventdPlugin *plugin)
{
    plugin->id = "sound";

    plugin->start = eventd_sound_start;
    plugin->stop = eventd_sound_stop;

    plugin->config_init = eventd_sound_config_init;
    plugin->config_clean = eventd_sound_config_clean;

    plugin->event_parse = eventd_sound_event_parse;
    plugin->event_action = eventd_sound_event_action;
}
