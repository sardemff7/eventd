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

#include <speak_lib.h>

#include <string.h>
#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <eventd-plugin.h>
#include <libeventd-event.h>
#include <libeventd-config.h>
#include <libeventd-regex.h>

#include "../pulseaudio.h"

#include "espeak.h"
#include "pulseaudio.h"

struct _EventdPluginContext {
    GHashTable *events;
};

typedef struct {
    guchar *data;
    gsize length;
} EventdSoundEspeakSoundData;

#define BUFFER_SIZE 2000

static void
_eventd_sound_espeak_callback_data_ref(EventdSoundEspeakCallbackData *data)
{
    ++data->ref_count;
}

static void
_eventd_sound_espeak_callback_data_free(EventdSoundEspeakCallbackData *data)
{
    EventdSoundEspeakSoundData *sound_data;

    if ( data->pong_mode )
    {
        sound_data = data->data;
        g_free(sound_data->data);
        g_free(sound_data);
    }
    else
        eventd_sound_espeak_pulseaudio_pa_data_free(data->data);

    g_free(data);
}

static void
_eventd_sound_espeak_callback_data_unref(EventdSoundEspeakCallbackData *data)
{
    if ( --data->ref_count > 0 )
        return;

    _eventd_sound_espeak_callback_data_free(data);
}

static EventdSoundEspeakCallbackData *
_eventd_sound_espeak_callback_data_new(gboolean pong_mode)
{
    EventdSoundEspeakCallbackData *data;
    EventdSoundEspeakSoundData *sound_data;

    data = g_new0(EventdSoundEspeakCallbackData, 1);

    data->ref_count = 1;
    data->pong_mode = pong_mode;
    if ( pong_mode )
    {
        _eventd_sound_espeak_callback_data_ref(data);
        sound_data = g_new0(EventdSoundEspeakSoundData, 1);
        sound_data->data = g_malloc0_n(0, sizeof(guchar));
        data->data = sound_data;
    }
    else
        data->data = eventd_sound_espeak_pulseaudio_pa_data_new();

    return data;
}

static int
synth_callback(gshort *wav, gint numsamples, espeak_EVENT *event)
{
    EventdSoundEspeakCallbackData *data;
    data = event->user_data;

    if ( data->pong_mode )
    {
        if ( wav != NULL )
        {
            EventdSoundEspeakSoundData *sound_data;
            gsize base;

            sound_data = data->data;
            base = sound_data->length;
            sound_data->length += numsamples * sizeof(gshort);
            sound_data->data = g_realloc_n(sound_data->data, sound_data->length, sizeof(guchar));
            memcpy(sound_data->data+base, wav, numsamples*sizeof(gshort));
        }
    }
    else
        eventd_sound_espeak_pulseaudio_play_data(wav, numsamples, event);

    if ( ( wav == NULL ) && ( event->type == espeakEVENT_LIST_TERMINATED ) )
        _eventd_sound_espeak_callback_data_unref(event->user_data);

    return 0;
}

static int
uri_callback(int type, const char *uri, const char *base)
{
    return 1;
}

static EventdPluginContext *
_eventd_sound_espeak_start(gpointer user_data)
{
    gint sample_rate;
    EventdPluginContext *context;

    sample_rate = espeak_Initialize(AUDIO_OUTPUT_RETRIEVAL, BUFFER_SIZE, NULL, 0);

    if ( sample_rate == EE_INTERNAL_ERROR )
    {
        g_warning("Couldn’t initialize eSpeak system");
        return NULL;
    }

    context = g_new0(EventdPluginContext, 1);

    espeak_SetSynthCallback(synth_callback);
    espeak_SetUriCallback(uri_callback);

    eventd_sound_espeak_pulseaudio_start(user_data, sample_rate);

    libeventd_regex_init();

    return context;
}

static void
_eventd_sound_espeak_stop(EventdPluginContext *context)
{
    libeventd_regex_clean();

    espeak_Synchronize();
    espeak_Terminate();

    g_free(context);
}

static void
_eventd_sound_espeak_event_parse(EventdPluginContext *context, const gchar *event_category, const gchar *event_name, GKeyFile *config_file)
{
    gboolean disable;
    gchar *message = NULL;
    gchar *name = NULL;
    gchar *parent = NULL;

    if ( ! g_key_file_has_group(config_file, "espeak") )
        return;

    if ( libeventd_config_key_file_get_boolean(config_file, "espeak", "disable", &disable) < 0 )
        return;
    if ( libeventd_config_key_file_get_string(config_file, "espeak", "message", &message) < 0 )
        return;

    name = libeventd_config_events_get_name(event_category, event_name);
    if ( event_name != NULL )
        parent = g_hash_table_lookup(context->events, event_category);

    if ( ! message )
        message = g_strdup(parent ?: "$text");

    if ( disable )
    {
        g_free(name);
        g_free(message);
    }
    else
        g_hash_table_insert(context->events, name, message);
}

static gboolean
_eventd_sound_espeak_regex_event_data_cb(const GMatchInfo *info, GString *r, gpointer event_data)
{
    gchar *name;
    gchar *data = NULL;

    name = g_match_info_fetch(info, 1);
    if ( event_data != NULL )
    {
        gchar *lang_name;
        gchar *lang_data = NULL;
        gchar *tmp = NULL;

        data = g_hash_table_lookup(event_data, name);

        lang_name = g_strconcat(name, "-lang", NULL);
        lang_data = g_hash_table_lookup(event_data, lang_name);
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
_eventd_sound_espeak_event_action(EventdPluginContext *context, EventdEvent *event)
{
    gchar *message;
    gchar *msg;
    espeak_ERROR error;
    EventdSoundEspeakCallbackData *data;

    message = libeventd_config_events_get_event(context->events, eventd_event_get_category(event), eventd_event_get_name(event));
    if ( message == NULL )
        return;

    msg = libeventd_regex_replace_event_data(message, eventd_event_get_data(event), _eventd_sound_espeak_regex_event_data_cb);

    data = _eventd_sound_espeak_callback_data_new(FALSE);
    error = espeak_Synth(msg, strlen(msg)+1, 0, POS_CHARACTER, 0, espeakCHARS_UTF8|espeakSSML, NULL, data);

    switch ( error )
    {
    case EE_INTERNAL_ERROR:
        g_warning("Couldn’t synthetise text");
        _eventd_sound_espeak_callback_data_free(data);
        goto fail;
    case EE_BUFFER_FULL:
    case EE_OK:
    default:
    break;
    }

fail:
    g_free(msg);
}

static void
_eventd_sound_espeak_event_pong(EventdPluginContext *context, EventdEvent *event)
{
    gchar *message;
    gchar *msg;
    espeak_ERROR error;
    EventdSoundEspeakCallbackData *data;
    EventdSoundEspeakSoundData *sound_data;

    message = libeventd_config_events_get_event(context->events, eventd_event_get_category(event), eventd_event_get_name(event));
    if ( message == NULL )
        return;

    msg = libeventd_regex_replace_event_data(message, eventd_event_get_data(event), _eventd_sound_espeak_regex_event_data_cb);

    data = _eventd_sound_espeak_callback_data_new(TRUE);
    error = espeak_Synth(msg, strlen(msg)+1, 0, POS_CHARACTER, 0, espeakCHARS_UTF8|espeakSSML, NULL, data);

    switch ( error )
    {
    case EE_INTERNAL_ERROR:
        g_warning("Couldn’t synthetise text");
        _eventd_sound_espeak_callback_data_free(data);
        goto fail;
    case EE_BUFFER_FULL:
    case EE_OK:
    default:
    break;
    }

    espeak_Synchronize();
    sound_data = data->data;
    eventd_event_add_pong_data(event, g_strdup("espeak-message"), msg);
    msg = NULL;
    eventd_event_add_pong_data(event, g_strdup("espeak-audio-data"), g_base64_encode(sound_data->data, sound_data->length));
    _eventd_sound_espeak_callback_data_unref(data);

fail:
    g_free(msg);
}

static void
_eventd_sound_espeak_config_init(EventdPluginContext *context)
{
    context->events = libeventd_config_events_new(g_free);
}

static void
_eventd_sound_espeak_config_clean(EventdPluginContext *context)
{
    g_hash_table_unref(context->events);
}

void
eventd_plugin_get_info(EventdPlugin *plugin)
{
    plugin->start = _eventd_sound_espeak_start;
    plugin->stop = _eventd_sound_espeak_stop;

    plugin->config_init = _eventd_sound_espeak_config_init;
    plugin->config_clean = _eventd_sound_espeak_config_clean;

    plugin->event_parse = _eventd_sound_espeak_event_parse;
    plugin->event_action = _eventd_sound_espeak_event_action;
    plugin->event_pong = _eventd_sound_espeak_event_pong;
}
