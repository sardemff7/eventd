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

#include <sndfile.h>

#include <glib.h>
#include <glib-object.h>

#include <eventd-plugin.h>
#include <libeventd-event.h>
#include <libeventd-regex.h>
#include <libeventd-config.h>

#include "sndfile-internal.h"
#include "pulseaudio.h"


struct _EventdPluginContext {
    GHashTable *events;
    EventdSndfilePulseaudioContext *pulseaudio;
};

typedef sf_count_t (*sndfile_readf_t)(SNDFILE *sndfile, void *ptr, sf_count_t frames);

static void
_eventd_sndfile_read_file(const gchar *filename, void **data, gsize *length, gint *format, guint32 *rate, guint8 *channels)
{
    SF_INFO sfi;
    SNDFILE *f = NULL;
    sndfile_readf_t readf_function = NULL;
    size_t factor = 1;
    sf_count_t r = 0;

    *data = NULL;
    *length = 0;
    *format = 0;
    *rate = 0;
    *channels = 0;

    if ( *filename == 0 )
        return;

    if ( ( f = sf_open(filename, SFM_READ, &sfi) ) == NULL )
    {
        g_warning("Can't open sound file");
        return;
    }

    switch ( sfi.format & SF_FORMAT_SUBMASK )
    {
    case SF_FORMAT_PCM_16:
    case SF_FORMAT_PCM_U8:
    case SF_FORMAT_PCM_S8:
        readf_function = (sndfile_readf_t) sf_readf_short;
        factor = sizeof(short);
    break;
    case SF_FORMAT_PCM_24:
    case SF_FORMAT_PCM_32:
        readf_function = (sndfile_readf_t) sf_readf_int;
        factor = sizeof(int);
    break;
    case SF_FORMAT_FLOAT:
    case SF_FORMAT_DOUBLE:
        readf_function = (sndfile_readf_t) sf_readf_float;
        factor = sizeof(float);
    break;
    default:
        g_warning("Unsupported format");
        goto out;
    }

    *length = (size_t) sfi.frames * sfi.channels * factor;

    *data = g_malloc0(*length);

    if ( ( r = readf_function(f, *data, sfi.frames) ) < 0 )
    {
        g_warning("Error while reading sound file");
        g_free(*data);
        *data = NULL;
        *length = 0;
    }
    else
    {
        *format = sfi.format & SF_FORMAT_SUBMASK;
        *rate = (guint32) sfi.samplerate;
        *channels = (guint8) sfi.channels;
    }

out:
    sf_close(f);
}

static void
_eventd_sndfile_event_action(EventdPluginContext *context, EventdEvent *event)
{
    EventdSndfileEvent *sndfile_event = NULL;
    gchar *file;
    gpointer data = NULL;
    gsize length = 0;
    gint format = 0;
    guint32 rate = 0;
    guint8 channels = 0;

    sndfile_event = libeventd_config_events_get_event(context->events, eventd_event_get_category(event), eventd_event_get_name(event));
    if ( sndfile_event == NULL )
        return;

    if ( sndfile_event->disable )
        return;


    if ( ( file = libeventd_config_get_filename(sndfile_event->sound, eventd_event_get_data(event), "sounds") ) != NULL )
        _eventd_sndfile_read_file(file, &data, &length, &format, &rate, &channels);
    // TODO: using event data

    eventd_sndfile_pulseaudio_play_data(context->pulseaudio, data, length, format, rate, channels);
}

static void
_eventd_sndfile_event_update(EventdSndfileEvent *event, gboolean disable, const gchar *sound)
{
    event->disable = disable;

    if ( sound != NULL )
    {
        g_free(event->sound);
        event->sound = g_strdup(sound);
    }
}

static EventdSndfileEvent *
_eventd_sndfile_event_new(gboolean disable, const gchar *sound, EventdSndfileEvent *parent)
{
    EventdSndfileEvent *event = NULL;

    sound = ( sound != NULL ) ? sound : ( parent != NULL ) ? parent->sound : "sound";

    event = g_new0(EventdSndfileEvent, 1);

    _eventd_sndfile_event_update(event, disable, sound);

    return event;
}

static void
_eventd_sndfile_event_parse(EventdPluginContext *context, const gchar *event_category, const gchar *event_name, GKeyFile *config_file)
{
    gboolean disable;
    gchar *sound = NULL;

    EventdSndfileEvent *event = NULL;
    gchar *name;

    if ( ! g_key_file_has_group(config_file, "sound") )
        return;

    if ( libeventd_config_key_file_get_boolean(config_file, "sound", "disable", &disable) < 0 )
        goto skip;
    if ( libeventd_config_key_file_get_string(config_file, "sound", "sound", &sound) < 0 )
        goto skip;

    name = libeventd_config_events_get_name(event_category, event_name);

    event = g_hash_table_lookup(context->events, name);
    if ( event != NULL )
    {
        _eventd_sndfile_event_update(event, disable, sound);
        g_free(name);
    }
    else
        g_hash_table_insert(context->events, name, _eventd_sndfile_event_new(disable, sound, g_hash_table_lookup(context->events, event_category)));

skip:
    g_free(sound);
}

static void
_eventd_sndfile_event_free(gpointer data)
{
    EventdSndfileEvent *event = data;

    g_free(event->sound);

    g_free(event);
}

static EventdPluginContext *
_eventd_sndfile_init(EventdCoreContext *core, EventdCoreInterface *interface)
{
    EventdPluginContext *context;

    context = g_new0(EventdPluginContext, 1);

    context->events = libeventd_config_events_new(_eventd_sndfile_event_free);

    context->pulseaudio = eventd_sndfile_pulseaudio_init();

    libeventd_regex_init();

    return context;
}

static void
_eventd_sndfile_uninit(EventdPluginContext *context)
{
    eventd_sndfile_pulseaudio_uninit(context->pulseaudio);

    libeventd_regex_clean();

    g_free(context);
}

static void
_eventd_sndfile_start(EventdPluginContext *context)
{
    eventd_sndfile_pulseaudio_start(context->pulseaudio);
}

static void
_eventd_sndfile_stop(EventdPluginContext *context)
{
    eventd_sndfile_pulseaudio_stop(context->pulseaudio);
}

static void
_eventd_sndfile_config_reset(EventdPluginContext *context)
{
    g_hash_table_remove_all(context->events);
}

void
eventd_plugin_get_info(EventdPlugin *plugin)
{
    plugin->init = _eventd_sndfile_init;
    plugin->uninit = _eventd_sndfile_uninit;

    plugin->start = _eventd_sndfile_start;
    plugin->stop = _eventd_sndfile_stop;

    plugin->config_reset = _eventd_sndfile_config_reset;

    plugin->event_parse = _eventd_sndfile_event_parse;
    plugin->event_action = _eventd_sndfile_event_action;
}
