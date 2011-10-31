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

#include <pulse/pulseaudio.h>
#include <speak_lib.h>

#include <string.h>
#include <glib.h>
#include <gio/gio.h>

#include <eventd-plugin.h>
#include <plugin-helper.h>

#include "../pulseaudio.h"

static GHashTable *events = NULL;

static pa_threaded_mainloop *pa_loop = NULL;
static pa_context *sound = NULL;

static pa_sample_spec sample_spec;

#define BUFFER_SIZE 2000

static void
pa_stream_state_callback(pa_stream *stream, void *userdata)
{
    pa_stream_state_t state = pa_stream_get_state(stream);
    switch  ( state )
    {
        case PA_STREAM_TERMINATED:
            //pa_stream_unref(stream);
        break;
        case PA_STREAM_READY:
            pa_threaded_mainloop_signal(pa_loop, 0);
        default:
        break;
    }
}

static void
pa_stream_drain_callback(pa_stream *stream, int success, void *userdata)
{
    pa_threaded_mainloop_signal(pa_loop, 0);
}

static int
synth_callback(gshort *wav, gint numsamples, espeak_EVENT *event)
{
    pa_stream *stream;

    stream = event->user_data;

    pa_threaded_mainloop_lock(pa_loop);

    if ( wav == NULL )
    {
        pa_operation *op;

        op = pa_stream_drain(stream, pa_stream_drain_callback, NULL);
        while ( pa_operation_get_state(op) == PA_OPERATION_RUNNING )
            pa_threaded_mainloop_wait(pa_loop);

        pa_operation_unref(op);

        pa_stream_disconnect(stream);
    }
    else
        pa_stream_write(stream, wav, numsamples*2, NULL, 0, PA_SEEK_RELATIVE);

    pa_threaded_mainloop_unlock(pa_loop);

    return 0;
}

static int
uri_callback(int type, const char *uri, const char *base)
{
    return 1;
}

void
eventd_pulseaudio_espeak_start(EventdPulseaudioContext *context)
{
    gint sample_rate;

    sample_rate = espeak_Initialize(AUDIO_OUTPUT_RETRIEVAL, BUFFER_SIZE, NULL, 0);

    if ( sample_rate == EE_INTERNAL_ERROR )
    {
        g_warning("Couldn’t initialize eSpeak system");
        return;
    }

    espeak_SetSynthCallback(synth_callback);
    espeak_SetUriCallback(uri_callback);

    sample_spec.rate = sample_rate;
    sample_spec.channels = 1;
    sample_spec.format = PA_SAMPLE_S16LE;

    pa_loop = context->pa_loop;
    sound = context->sound;

    eventd_plugin_helper_regex_init();
}

void
eventd_pulseaudio_espeak_stop()
{
    eventd_plugin_helper_regex_clean();

    espeak_Synchronize();
    espeak_Terminate();
}

static void
eventd_pulseaudio_espeak_event_parse(const gchar *client_type, const gchar *event_type, GKeyFile *config_file)
{
    gchar *message = NULL;
    gchar *name = NULL;
    gchar *parent = NULL;

    if ( ! g_key_file_has_group(config_file, "espeak") )
        return;

    if ( eventd_plugin_helper_config_key_file_get_string(config_file, "espeak", "message", &message) < 0 )
        return;

    if ( event_type != NULL )
    {
        parent = g_hash_table_lookup(events, client_type);
        name = g_strconcat(client_type, "-", event_type, NULL);
    }
    else
        name = g_strdup(client_type);

    if ( ! message )
        message = g_strdup(parent ?: "$event-data[text]");

    g_hash_table_insert(events, name, message);
}

static gboolean
eventd_pulseaudio_espeak_regex_event_data_cb(const GMatchInfo *info, GString *r, gpointer event_data)
{
    gchar *name;
    gchar *data = NULL;

    name = g_match_info_fetch(info, 1);
    if ( event_data != NULL )
    {
        gchar *lang_name;
        gchar *lang_data = NULL;
        gchar *tmp = NULL;

        data = g_hash_table_lookup((GHashTable *)event_data, name);

        lang_name = g_strconcat(name, "-lang", NULL);
        lang_data = g_hash_table_lookup((GHashTable *)event_data, lang_name);
        g_free(lang_name);

        if ( lang_data != NULL )
        {
            tmp = data;
            data = g_strdup_printf("<voice name=\"%s\">%s</voice>", lang_data, tmp);
        }
        else
            data = g_strdup(data);
    }
    g_free(name);

    g_string_append(r, data ?: "");
    g_free(data);

    return FALSE;
}

static void
eventd_pulseaudio_espeak_event_action(const gchar *client_type, const gchar *client_name, const gchar *event_type, GHashTable *event_data)
{
    gchar *name;
    gchar *message;
    gchar *msg;
    espeak_ERROR error;
    pa_stream *stream;

    name = g_strconcat(client_type, "-", event_type, NULL);

    message = g_hash_table_lookup(events, name);
    if ( ( message == NULL ) && ( ( message = g_hash_table_lookup(events, client_type) ) == NULL ) )
        goto fail;

    msg = eventd_plugin_helper_regex_replace_event_data(message, event_data, eventd_pulseaudio_espeak_regex_event_data_cb);

    stream = pa_stream_new(sound, "eSpeak eventd message", &sample_spec, NULL);
    pa_stream_set_state_callback(stream, pa_stream_state_callback, NULL);
    pa_stream_connect_playback(stream, NULL, NULL, 0, NULL, NULL);

    pa_threaded_mainloop_lock(pa_loop);
    pa_threaded_mainloop_wait(pa_loop);

    error = espeak_Synth(msg, strlen(msg)+1, 0, POS_CHARACTER, 0, espeakCHARS_UTF8|espeakSSML, NULL, stream);
    g_free(msg);

    switch ( error )
    {
    case EE_INTERNAL_ERROR:
        pa_stream_disconnect(stream);
        g_warning("Couldn’t synthetise text");
    case EE_BUFFER_FULL:
    case EE_OK:
    default:
    break;
    }

    pa_threaded_mainloop_unlock(pa_loop);

fail:
    g_free(name);
}

static void
eventd_pulseaudio_espeak_config_init()
{
    events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
}

static void
eventd_pulseaudio_espeak_config_clean()
{
    g_hash_table_unref(events);
}

void
eventd_plugin_get_info(EventdPlugin *plugin)
{
    plugin->start = (EventdPluginStartFunc)eventd_pulseaudio_espeak_start;
    plugin->stop = eventd_pulseaudio_espeak_stop;

    plugin->config_init = eventd_pulseaudio_espeak_config_init;
    plugin->config_clean = eventd_pulseaudio_espeak_config_clean;

    plugin->event_parse = eventd_pulseaudio_espeak_event_parse;
    plugin->event_action = eventd_pulseaudio_espeak_event_action;
}
