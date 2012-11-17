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

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <glib.h>
#include <glib-object.h>

#include <speak_lib.h>

#include <eventd-plugin.h>
#include <libeventd-event.h>
#include <libeventd-config.h>
#include <libeventd-regex.h>

struct _EventdPluginContext {
    GHashTable *events;
};

#define BUFFER_SIZE 2000


/*
 * Initialization interface
 */

static EventdPluginContext *
_eventd_espeak_init(EventdCoreContext *core, EventdCoreInterface *interface)
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

    context->events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    libeventd_regex_init();

    return context;
}

static void
_eventd_espeak_uninit(EventdPluginContext *context)
{
    g_hash_table_unref(context->events);

    libeventd_regex_clean();

    espeak_Terminate();

    g_free(context);
}


/*
 * Start/Stop interface
 */

static void
_eventd_espeak_stop(EventdPluginContext *context)
{
    espeak_Synchronize();
}


/*
 * Configuration interface
 */

static void
_eventd_espeak_event_parse(EventdPluginContext *context, const gchar *id, GKeyFile *config_file)
{
    gboolean disable;
    gchar *message = NULL;

    if ( ! g_key_file_has_group(config_file, "Espeak") )
        return;

    if ( libeventd_config_key_file_get_boolean(config_file, "Espeak", "Disable", &disable) < 0 )
        return;

    if ( ! disable )
    {
        if ( libeventd_config_key_file_get_string(config_file, "Espeak", "Message", &message) < 0 )
            return;
        if ( message == NULL )
            message = g_strdup("$message");
    }

    g_hash_table_insert(context->events, g_strdup(id), message);
}

static void
_eventd_espeak_config_reset(EventdPluginContext *context)
{
    g_hash_table_remove_all(context->events);
}


/*
 * Event action interface
 */

static gchar *
_eventd_espeak_regex_event_data_cb(const gchar *name, EventdEvent *event, gpointer user_data)
{
    const gchar *data;
    gchar *ret;

    data = eventd_event_get_data(event, name);
    if ( data != NULL )
    {
        gchar *lang_name;
        const gchar *lang_data;

        lang_name = g_strconcat(name, "-lang", NULL);
        lang_data = eventd_event_get_data(event, lang_name);
        g_free(lang_name);

        if ( lang_data != NULL )
        {
            ret = g_strdup_printf("<voice name=\"%s\">%s</voice>", lang_data, data);
        }
        else
            ret = g_strdup(data);
    }
    else
        ret = g_strdup("");

    return ret;
}


static void
_eventd_espeak_event_action(EventdPluginContext *context, const gchar *config_id, EventdEvent *event)
{
    gchar *message;
    gchar *msg;
    espeak_ERROR error;

    message = g_hash_table_lookup(context->events, config_id);
    if ( message == NULL )
        return;

    msg = libeventd_regex_replace_event_data(message, event, _eventd_espeak_regex_event_data_cb, NULL);

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

EVENTD_EXPORT const gchar *eventd_plugin_id = "eventd-espeak";
EVENTD_EXPORT
void
eventd_plugin_get_interface(EventdPluginInterface *interface)
{
    interface->init   = _eventd_espeak_init;
    interface->uninit = _eventd_espeak_uninit;

    interface->stop = _eventd_espeak_stop;

    interface->event_parse  = _eventd_espeak_event_parse;
    interface->config_reset = _eventd_espeak_config_reset;

    interface->event_action = _eventd_espeak_event_action;
}
