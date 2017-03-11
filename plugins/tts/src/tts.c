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

#include <libspeechd.h>

#include "eventd-plugin.h"
#include "libeventd-event.h"
#include "libeventd-helpers-config.h"

struct _EventdPluginContext {
    SPDConnection *spd;
    GSList *actions;
};

struct _EventdPluginAction {
    FormatString *message;
};


static void
_eventd_tts_action_free(gpointer data)
{
    EventdPluginAction *action = data;

    evhelpers_format_string_unref(action->message);

    g_slice_free(EventdPluginAction, action);
}

/*
 * Initialization interface
 */

static EventdPluginContext *
_eventd_tts_init(EventdPluginCoreContext *core)
{
    SPDConnection *spd;
    EventdPluginContext *context;

    spd = spd_open(PACKAGE_NAME, NULL, NULL, SPD_MODE_THREADED);

    if ( spd == NULL )
    {
        g_warning("Couldn't initialize Speech Dispatcher connection");
        return NULL;
    }

    context = g_new0(EventdPluginContext, 1);
    context->spd = spd;

    return context;
}

static void
_eventd_tts_uninit(EventdPluginContext *context)
{
    spd_close(context->spd);

    g_free(context);
}


/*
 * Start/Stop interface
 */

static void
_eventd_tts_stop(EventdPluginContext *context)
{
    spd_cancel(context->spd);
}


/*
 * Configuration interface
 */

static EventdPluginAction *
_eventd_tts_action_parse(EventdPluginContext *context, GKeyFile *config_file)
{
    gboolean disable = FALSE;
    FormatString *message = NULL;

    if ( ! g_key_file_has_group(config_file, "TTS") )
        return NULL;

    if ( evhelpers_config_key_file_get_boolean(config_file, "TTS", "Disable", &disable) < 0 )
        return NULL;

    if ( disable )
        return NULL;

    if ( evhelpers_config_key_file_get_locale_format_string_with_default(config_file, "TTS", "Message", NULL, "${message}", &message) < 0 )
        return NULL;

    EventdPluginAction *action;
    action = g_slice_new(EventdPluginAction);
    action->message = message;

    context->actions = g_slist_prepend(context->actions, action);

    return action;
}

static void
_eventd_tts_config_reset(EventdPluginContext *context)
{
    g_slist_free_full(context->actions, _eventd_tts_action_free);
    context->actions = NULL;
}


/*
 * Event action interface
 */

static void
_eventd_tts_event_action(EventdPluginContext *context, EventdPluginAction *action, EventdEvent *event)
{
    gchar *msg;
    gint id;

    msg = evhelpers_format_string_get_string(action->message, event, NULL, NULL);

    id = spd_say(context->spd, SPD_NOTIFICATION, msg);

    if ( id == -1 )
        g_warning("Couldn't synthetise text");

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

    eventd_plugin_interface_add_action_parse_callback(interface, _eventd_tts_action_parse);
    eventd_plugin_interface_add_config_reset_callback(interface, _eventd_tts_config_reset);

    eventd_plugin_interface_add_event_action_callback(interface, _eventd_tts_event_action);
}
