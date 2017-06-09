/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2017 Quentin "Sardem FF7" Glidic
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

#include "config.h"

#include <string.h>

#include <glib.h>
#include <glib-object.h>

#include <nkutils-xdg-theme.h>
#include <sndfile.h>

#include "eventd-plugin.h"
#include "libeventd-event.h"
#include "libeventd-helpers-config.h"

#include "pulseaudio.h"


struct _EventdPluginContext {
    GSList *actions;
    EventdSoundPulseaudioContext *pulseaudio;
    NkXdgThemeContext *theme_context;
};

struct _EventdPluginAction {
    Filename *sound;
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

static void
_eventd_sound_action_free(gpointer data)
{
    EventdPluginAction *action = data;

    evhelpers_filename_unref(action->sound);

    g_slice_free(EventdPluginAction, action);
}

/*
 * Initialization interface
 */

static EventdPluginContext *
_eventd_sound_init(EventdPluginCoreContext *core)
{
    EventdPluginContext *context;

    context = g_new0(EventdPluginContext, 1);

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
    context->theme_context = nk_xdg_theme_context_new();
}

static void
_eventd_sound_stop(EventdPluginContext *context)
{
    nk_xdg_theme_context_free(context->theme_context);
    eventd_sound_pulseaudio_stop(context->pulseaudio);
}

/*
 * Configuration interface
 */

static EventdPluginAction *
_eventd_sound_action_parse(EventdPluginContext *context, GKeyFile *config_file)
{
    gboolean disable = FALSE;
    Filename *sound = NULL;

    if ( ! g_key_file_has_group(config_file, "Sound") )
        return NULL;

    if ( evhelpers_config_key_file_get_boolean(config_file, "Sound", "Disable", &disable) < 0 )
        return NULL;

    if ( disable )
        return NULL;
    if ( evhelpers_config_key_file_get_filename_with_default(config_file, "Sound", "Sound", "sound", &sound) < 0 )
        return NULL;

    EventdPluginAction *action;
    action = g_slice_new(EventdPluginAction);
    action->sound = sound;

    context->actions = g_slist_prepend(context->actions, action);
    return action;
}

static void
_eventd_sound_config_reset(EventdPluginContext *context)
{
    g_slist_free_full(context->actions, _eventd_sound_action_free);
    context->actions = NULL;
}


/*
 * Event action interface
 */

static void
_eventd_sound_event_action(EventdPluginContext *context, EventdPluginAction *action, EventdEvent *event)
{
    gchar *uri;
    GVariant *var;
    gpointer data = NULL;
    gsize length = 0;
    gint format = 0;
    guint32 rate = 0;
    guint8 channels = 0;

    switch ( evhelpers_filename_process(action->sound, event, "sounds", &uri, &var) )
    {
    case FILENAME_PROCESS_RESULT_URI:
        if ( g_str_has_prefix(uri, "file://"))
            _eventd_sound_read_file(uri + strlen("file://"), &data, &length, &format, &rate, &channels);
        g_free(uri);
    break;
    case FILENAME_PROCESS_RESULT_DATA:
        /* TODO: using event data */
        g_variant_unref(var);
    break;
    case FILENAME_PROCESS_RESULT_THEME:
    {
        const gchar *themes[2] = { NULL, NULL };
        gchar *name = uri + strlen("theme:");

        gchar *c;
        if ( ( c = g_utf8_strchr(name, -1, '/') ) != NULL )
        {
            *c = '\0';
            themes[0] = name;
            name = ++c;
        }

        gchar *file;
        file = nk_xdg_theme_get_sound(context->theme_context, themes, name, NULL, NULL);
        if ( file != NULL )
            _eventd_sound_read_file(file, &data, &length, &format, &rate, &channels);
        g_free(file);
        g_free(uri);
    }
    break;
    case FILENAME_PROCESS_RESULT_NONE:
    break;
    }

    eventd_sound_pulseaudio_play_data(context->pulseaudio, data, length, format, rate, channels);
}


/*
 * Plugin interface
 */

EVENTD_EXPORT const gchar *eventd_plugin_id = "sound";
EVENTD_EXPORT
void
eventd_plugin_get_interface(EventdPluginInterface *interface)
{
    eventd_plugin_interface_add_init_callback(interface, _eventd_sound_init);
    eventd_plugin_interface_add_uninit_callback(interface, _eventd_sound_uninit);

    eventd_plugin_interface_add_start_callback(interface, _eventd_sound_start);
    eventd_plugin_interface_add_stop_callback(interface, _eventd_sound_stop);

    eventd_plugin_interface_add_action_parse_callback(interface, _eventd_sound_action_parse);
    eventd_plugin_interface_add_config_reset_callback(interface, _eventd_sound_config_reset);

    eventd_plugin_interface_add_event_action_callback(interface, _eventd_sound_event_action);
}
