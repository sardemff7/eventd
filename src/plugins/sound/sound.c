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
#include <glib-object.h>

#include <eventd-plugin.h>
#include <libeventd-plugins.h>

#include "pulseaudio.h"

struct _EventdPluginContext {
    GList *plugins;
    EventdSoundPulseaudioContext *context;
};

static EventdPluginContext *
_eventd_sound_start(gpointer user_data)
{
    EventdPluginContext *context;

    context = g_new0(EventdPluginContext, 1);

    context->context = eventd_sound_pulseaudio_start();
    libeventd_plugins_load(&context->plugins, "plugins" G_DIR_SEPARATOR_S "sound", context->context);

    return context;
}

static void
_eventd_sound_stop(EventdPluginContext *context)
{
    libeventd_plugins_unload(&context->plugins);
    eventd_sound_pulseaudio_stop(context->context);
    g_free(context);
}

static void
_eventd_sound_event_action(EventdPluginContext *context, EventdEvent *event)
{
    libeventd_plugins_event_action_all(context->plugins, event);
}

static void
_eventd_sound_event_parse(EventdPluginContext *context, const gchar *type, const gchar *event, GKeyFile *config_file)
{
    libeventd_plugins_event_parse_all(context->plugins, type, event, config_file);
}

static void
_eventd_sound_config_init(EventdPluginContext *context)
{
    libeventd_plugins_config_init_all(context->plugins);
}

static void
_eventd_sound_config_clean(EventdPluginContext *context)
{
    libeventd_plugins_config_clean_all(context->plugins);
}

void
eventd_plugin_get_info(EventdPlugin *plugin)
{
    plugin->start = _eventd_sound_start;
    plugin->stop = _eventd_sound_stop;

    plugin->config_init = _eventd_sound_config_init;
    plugin->config_clean = _eventd_sound_config_clean;

    plugin->event_parse = _eventd_sound_event_parse;
    plugin->event_action = _eventd_sound_event_action;
}
