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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <glib.h>
#include <glib-object.h>

#include <sndfile.h>

#include <eventd-plugin.h>
#include <libeventd-event.h>
#include <libeventd-config.h>

#include "pulseaudio.h"


struct _EventdPluginContext {
    GHashTable *events;
    EventdSoundPulseaudioContext *pulseaudio;
};

/*
 * File reading helper
 */
typedef sf_count_t (*sndfile_readf_t)(SNDFILE *sndfile, void *ptr, sf_count_t frames);

static void
_eventd_sound_read_file(const gchar *filename, void **data, gsize *length, gint *format, guint32 *rate, guint8 *channels)
{
    *data = NULL;
    *length = 0;
    *format = 0;
    *rate = 0;
    *channels = 0;

    if ( *filename == 0 )
        return;

    SNDFILE *f;
    SF_INFO sfi;
    if ( ( f = sf_open(filename, SFM_READ, &sfi) ) == NULL )
    {
        g_warning("Can't open sound file");
        return;
    }

    sndfile_readf_t readf_function;
    size_t factor;
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

    if ( readf_function(f, *data, sfi.frames) < 0 )
    {
        g_warning("Error while reading sound file: %s", sf_strerror(f));
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


/*
 * Initialization interface
 */

static EventdPluginContext *
_eventd_sound_init(EventdCoreContext *core, EventdCoreInterface *interface)
{
    EventdPluginContext *context;

    context = g_new0(EventdPluginContext, 1);

    context->events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)libeventd_filename_unref);

    context->pulseaudio = eventd_sound_pulseaudio_init();

    return context;
}

static void
_eventd_sound_uninit(EventdPluginContext *context)
{
    eventd_sound_pulseaudio_uninit(context->pulseaudio);

    g_free(context);
}


/*
 * Start/Stop interface
 */

static void
_eventd_sound_start(EventdPluginContext *context)
{
    eventd_sound_pulseaudio_start(context->pulseaudio);
}

static void
_eventd_sound_stop(EventdPluginContext *context)
{
    eventd_sound_pulseaudio_stop(context->pulseaudio);
}

/*
 * Configuration interface
 */

static void
_eventd_sound_event_parse(EventdPluginContext *context, const gchar *id, GKeyFile *config_file)
{
    gboolean disable;
    LibeventdFilename *sound = NULL;

    if ( ! g_key_file_has_group(config_file, "Sound") )
        return;

    if ( libeventd_config_key_file_get_boolean(config_file, "Sound", "Disable", &disable) < 0 )
        return;

    if ( ! disable )
    {
        if ( libeventd_config_key_file_get_filename_with_default(config_file, "Sound", "File", "sound-file", &sound) < 0 )
            return;
    }

    g_hash_table_insert(context->events, g_strdup(id), sound);
}

static void
_eventd_sound_config_reset(EventdPluginContext *context)
{
    g_hash_table_remove_all(context->events);
}


/*
 * Event action interface
 */

static void
_eventd_sound_event_action(EventdPluginContext *context, const gchar *config_id, EventdEvent *event)
{
    LibeventdFilename *sound;
    gchar *file;
    gpointer data = NULL;
    gsize length = 0;
    gint format = 0;
    guint32 rate = 0;
    guint8 channels = 0;

    sound = g_hash_table_lookup(context->events, config_id);
    if ( sound == NULL )
        return;

    if ( libeventd_filename_get_path(sound, event, "sounds", NULL, &file) )
    {
        if ( file != NULL )
            _eventd_sound_read_file(file, &data, &length, &format, &rate, &channels);
        // TODO: using event data
        g_free(file);
    }

    eventd_sound_pulseaudio_play_data(context->pulseaudio, data, length, format, rate, channels);
}


/*
 * Plugin interface
 */

EVENTD_EXPORT const gchar *eventd_plugin_id = "eventd-sound";
EVENTD_EXPORT
void
eventd_plugin_get_interface(EventdPluginInterface *interface)
{
    libeventd_plugin_interface_add_init_callback(interface, _eventd_sound_init);
    libeventd_plugin_interface_add_uninit_callback(interface, _eventd_sound_uninit);

    libeventd_plugin_interface_add_start_callback(interface, _eventd_sound_start);
    libeventd_plugin_interface_add_stop_callback(interface, _eventd_sound_stop);

    libeventd_plugin_interface_add_event_parse_callback(interface, _eventd_sound_event_parse);
    libeventd_plugin_interface_add_config_reset_callback(interface, _eventd_sound_config_reset);

    libeventd_plugin_interface_add_event_action_callback(interface, _eventd_sound_event_action);
}
