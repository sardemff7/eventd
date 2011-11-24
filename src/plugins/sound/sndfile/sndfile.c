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

#include <sndfile.h>

#include <glib.h>
#include <glib-object.h>

#include <eventd-plugin.h>
#include <libeventd-event.h>
#include <libeventd-client.h>
#include <libeventd-regex.h>
#include <libeventd-config.h>

#include "../pulseaudio.h"
#include "sndfile-internal.h"
#include "pulseaudio.h"


static GHashTable *events = NULL;

typedef sf_count_t (*sndfile_readf_t)(SNDFILE *sndfile, void *ptr, sf_count_t frames);

static gboolean
_eventd_sound_sndfile_read_file(EventdSoundSndfileEvent *event, const gchar *filename)
{
    gboolean ret = FALSE;
    SF_INFO sfi;
    SNDFILE* f = NULL;
    sndfile_readf_t readf_function = NULL;
    size_t factor = 1;
    sf_count_t r = 0;

    if ( ( f = sf_open(filename, SFM_READ, &sfi) ) == NULL )
    {
        g_warning("Can't open sound file");
        return ret;
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

    event->length = (size_t) sfi.frames * sfi.channels * factor;

    event->data = g_malloc0(event->length);

    if ( ( r = readf_function(f, event->data, sfi.frames) ) < 0 )
        g_warning("Error while reading sound file");
    else
    {
        ret = TRUE;
        eventd_sound_sndfile_pulseaudio_create_sample(event, &sfi);
    }

out:
    sf_close(f);

    return ret;
}

static GHashTable *
_eventd_sound_sndfile_event_action(EventdClient *client, EventdEvent *event)
{
    EventdSoundSndfileEvent *sndfile_event = NULL;

    sndfile_event = libeventd_config_events_get_event(events, libeventd_client_get_type(client), eventd_event_get_type(event));
    if ( sndfile_event == NULL )
        return NULL;

    if ( sndfile_event->disable )
        return NULL;

    return NULL;
}

static void
_eventd_sound_sndfile_event_clean(EventdSoundSndfileEvent *event)
{
    g_free(event->data);
    event->length = 0;
    eventd_sound_sndfile_pulseaudio_remove_sample(event->sample);
    g_free(event->sample);
}

static gboolean
_eventd_sound_sndfile_event_update(EventdSoundSndfileEvent *event, gboolean disable, const gchar *filename, EventdSoundSndfileEvent *parent)
{
    gboolean ret = TRUE;
    gchar *real_filename = NULL;

    event->disable = disable;

    if ( disable )
        _eventd_sound_sndfile_event_clean(event);
    else if ( filename != NULL )
    {
        if ( g_path_is_absolute(filename) )
            real_filename = g_strdup(filename);
        else
            real_filename = g_build_filename(g_get_user_data_dir(), PACKAGE_NAME, "sounds", filename, NULL);

        ret = _eventd_sound_sndfile_read_file(event, real_filename);
        g_free(real_filename);
    }
    else if ( parent != NULL )
    {
        event->length = parent->length;
        event->data = g_memdup(parent->data, event->length);
    }

    return ret;
}

static EventdSoundSndfileEvent *
_eventd_sound_sndfile_event_new(gboolean disable, const gchar *name, const gchar *sample, const gchar *filename, EventdSoundSndfileEvent *parent)
{
    EventdSoundSndfileEvent *event = NULL;

    event = g_new0(EventdSoundSndfileEvent, 1);

    event->sample = g_strdup(sample);
    if ( ( parent != NULL ) && ( filename == NULL ) )
    {
        event->length = parent->length;
        event->data = g_memdup(parent->data, event->length);
    }

    if ( ! _eventd_sound_sndfile_event_update(event, disable, filename, parent) )
        event = (g_free(event), NULL);

    return event;
}

static void
_eventd_sound_sndfile_event_parse(const gchar *client_type, const gchar *event_type, GKeyFile *config_file)
{
    gboolean disable;
    gchar *filename = NULL;

    EventdSoundSndfileEvent *event = NULL;
    gchar *name;

    if ( ! g_key_file_has_group(config_file, "sound") )
        return;

    if ( libeventd_config_key_file_get_boolean(config_file, "sound", "disable", &disable) < 0 )
        goto skip;
    if ( libeventd_config_key_file_get_string(config_file, "sound", "file", &filename) < 0 )
        goto skip;

    name = libeventd_config_events_get_name(client_type, event_type);

    event = g_hash_table_lookup(events, name);
    if ( event != NULL )
    {
        if ( ! _eventd_sound_sndfile_event_update(event, disable, filename, NULL) )
        {
            g_hash_table_remove(events, name);
            g_free(event);
        }
        g_free(name);
    }
    else
    {
        event = _eventd_sound_sndfile_event_new(disable, name, name, filename, g_hash_table_lookup(events, client_type));
        if ( event != NULL )
            g_hash_table_insert(events, name, event);
        else
        {
            g_free(event);
            g_free(name);
        }
    }

skip:
    g_free(filename);
}

static void
_eventd_sound_sndfile_event_free(gpointer data)
{
    EventdSoundSndfileEvent *event = data;

    _eventd_sound_sndfile_event_clean(event);
    g_free(event);
}

static void
_eventd_sound_sndfile_start(gpointer user_data)
{
    eventd_sound_sndfile_pulseaudio_start(user_data);
}

static void
_eventd_sound_sndfile_config_init()
{
    events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, _eventd_sound_sndfile_event_free);
}

static void
_eventd_sound_sndfile_config_clean()
{
    g_hash_table_unref(events);
}

void
eventd_plugin_get_info(EventdPlugin *plugin)
{
    plugin->id = "sndfile";

    plugin->start = _eventd_sound_sndfile_start;

    plugin->config_init = _eventd_sound_sndfile_config_init;
    plugin->config_clean = _eventd_sound_sndfile_config_clean;

    plugin->event_parse = _eventd_sound_sndfile_event_parse;
    plugin->event_action = _eventd_sound_sndfile_event_action;
}
