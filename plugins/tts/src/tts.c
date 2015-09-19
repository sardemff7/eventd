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

#include <config.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <glib.h>
#include <glib-object.h>

#include <speak_lib.h>

#include <eventd-plugin.h>
#include <libeventd-event.h>
#include <libeventd-helpers-config.h>

struct _EventdPluginContext {
    GHashTable *events;
};

#define BUFFER_SIZE 2000


/*
 * Initialization interface
 */

static EventdPluginContext *
_eventd_tts_init(EventdPluginCoreContext *core, EventdPluginCoreInterface *interface)
{
    gint sample_rate;
    EventdPluginContext *context;

    sample_rate = espeak_Initialize(AUDIO_OUTPUT_PLAYBACK, BUFFER_SIZE, NULL, 0);

    if ( sample_rate == EE_INTERNAL_ERROR )
    {
        g_warning("Couldn't initialize eSpeak system");
        return NULL;
    }

    context = g_new0(EventdPluginContext, 1);

    context->events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)evhelpers_format_string_unref);

    return context;
}

static void
_eventd_tts_uninit(EventdPluginContext *context)
{
    g_hash_table_unref(context->events);

    espeak_Terminate();

    g_free(context);
}


/*
 * Start/Stop interface
 */

static void
_eventd_tts_stop(EventdPluginContext *context)
{
    espeak_Synchronize();
}


/*
 * Configuration interface
 */

static void
_eventd_tts_event_parse(EventdPluginContext *context, const gchar *id, GKeyFile *config_file)
{
    gboolean disable;
    FormatString *message = NULL;

    if ( ! g_key_file_has_group(config_file, "TTS") )
        return;

    if ( evhelpers_config_key_file_get_boolean(config_file, "TTS", "Disable", &disable) < 0 )
        return;

    if ( ! disable )
    {
        if ( evhelpers_config_key_file_get_locale_format_string_with_default(config_file, "TTS", "Message", NULL, "<voice name=\"${message-lang}\">${message}</voice>", &message) < 0 )
            return;
    }

    g_hash_table_insert(context->events, g_strdup(id), message);
}

static void
_eventd_tts_config_reset(EventdPluginContext *context)
{
    g_hash_table_remove_all(context->events);
}


/*
 * Event action interface
 */

static void
_eventd_tts_event_action(EventdPluginContext *context, const gchar *config_id, EventdEvent *event)
{
    const FormatString *message;
    gchar *msg;
    espeak_ERROR error;

    message = g_hash_table_lookup(context->events, config_id);
    if ( message == NULL )
        return;

    msg = evhelpers_format_string_get_string(message, event, NULL, NULL);

    error = espeak_Synth(msg, strlen(msg)+1, 0, POS_CHARACTER, 0, espeakCHARS_UTF8|espeakSSML, NULL, NULL);

    switch ( error )
    {
    case EE_INTERNAL_ERROR:
    case EE_BUFFER_FULL:
        g_warning("Couldn't synthetise text");
    case EE_OK:
    break;
    case EE_NOT_FOUND:
        g_assert_not_reached();
    }

    g_free(msg);
}


/*
 * Plugin interface
 */

EVENTD_EXPORT const gchar *eventd_plugin_id = "tts";
EVENTD_EXPORT
void
eventd_plugin_get_interface(EventdPluginInterface *interface)
{
    eventd_plugin_interface_add_init_callback(interface, _eventd_tts_init);
    eventd_plugin_interface_add_uninit_callback(interface, _eventd_tts_uninit);

    eventd_plugin_interface_add_stop_callback(interface, _eventd_tts_stop);

    eventd_plugin_interface_add_event_parse_callback(interface, _eventd_tts_event_parse);
    eventd_plugin_interface_add_config_reset_callback(interface, _eventd_tts_config_reset);

    eventd_plugin_interface_add_event_action_callback(interface, _eventd_tts_event_action);
}
