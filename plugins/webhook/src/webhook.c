/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2018 Quentin "Sardem FF7" Glidic
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
#include <libsoup/soup.h>

#include "nkutils-git-version.h"

#include "eventd-plugin.h"
#include "libeventd-helpers-config.h"

struct _EventdPluginContext {
    gboolean no_user_agent;
    GSList *actions;
};

struct _EventdPluginAction {
    FormatString *url;
    FormatString *string;
};

static void
_eventd_webhook_action_free(gpointer data)
{
    EventdPluginAction *action = data;

    evhelpers_format_string_unref(action->string);
    evhelpers_format_string_unref(action->url);

    g_slice_free(EventdPluginAction, action);
}

/*
 * Initialization interface
 */

static EventdPluginContext *
_eventd_webhook_init(EventdPluginCoreContext *core)
{
    EventdPluginContext *context;

    context = g_new0(EventdPluginContext, 1);

    return context;
}

static void
_eventd_webhook_uninit(EventdPluginContext *context)
{
    g_free(context);
}


/*
 * Configuration interface
 */

static void
_eventd_webhook_global_parse(EventdPluginContext *context, GKeyFile *config_file)
{
    if ( ! g_key_file_has_group(config_file, "WebHook") )
        return;

    gboolean no_user_agent = FALSE;
    if ( evhelpers_config_key_file_get_boolean(config_file, "WebHook", "NoUserAgent", &no_user_agent) == 0 )
        context->no_user_agent = no_user_agent;
}

static EventdPluginAction *
_eventd_webhook_action_parse(EventdPluginContext *context, GKeyFile *config_file)
{
    gint r;
    gboolean disable = FALSE;
    FormatString *url = NULL;
    FormatString *string = NULL;

    if ( ! g_key_file_has_group(config_file, "WebHook") )
        return NULL;

    if ( evhelpers_config_key_file_get_boolean(config_file, "WebHook", "Disable", &disable) < 0 )
        return NULL;

    if ( disable )
        return NULL;

    if ( evhelpers_config_key_file_get_format_string(config_file, "WebHook", "URL", &url) != 0 )
        goto fail;

    r = evhelpers_config_key_file_get_template(config_file, "WebHook", "Template", &string);
    if ( r < 0 )
        goto fail;

    if ( ( r > 0 ) && ( evhelpers_config_key_file_get_format_string(config_file, "WebHook", "String", &string) != 0 ) )
        goto fail;

    EventdPluginAction *action;
    action = g_slice_new(EventdPluginAction);
    action->url = url;
    action->string = string;

    context->actions = g_slist_prepend(context->actions, action);

    return action;

fail:
    evhelpers_format_string_unref(string);
    evhelpers_format_string_unref(url);
    return NULL;
}

static void
_eventd_webhook_config_reset(EventdPluginContext *context)
{
    g_slist_free_full(context->actions, _eventd_webhook_action_free);
    context->actions = NULL;
}


/*
 * Event action interface
 */

static void
_eventd_webhook_message_callback(SoupSession *session, SoupMessage *msg, gpointer user_data)
{
    if ( SOUP_STATUS_IS_SUCCESSFUL(msg->status_code) )
        return;

    SoupURI *uri;

    uri = soup_message_get_uri(msg);
    g_warning("Message to %s://%s%s%s%s is not successful: (%d %s) %s", soup_uri_get_scheme(uri), soup_uri_get_host(uri), soup_uri_get_path(uri), soup_uri_get_query(uri), soup_uri_get_fragment(uri),msg->status_code, soup_status_get_phrase(msg->status_code), msg->reason_phrase);
}

static void
_eventd_webhook_event_action(EventdPluginContext *context, EventdPluginAction *action, EventdEvent *event)
{
    gchar *url = NULL;
    gchar *string = NULL;
    SoupSession *session;
    SoupMessage *msg;

    url = evhelpers_format_string_get_string(action->url, event, NULL, NULL);
    string = evhelpers_format_string_get_string(action->string, event, NULL, NULL);

    session = soup_session_new();
    if ( ! context->no_user_agent )
        g_object_set(session, SOUP_SESSION_USER_AGENT, PACKAGE_NAME " " NK_PACKAGE_VERSION, NULL);
    msg = soup_message_new(SOUP_METHOD_POST, url);
    soup_message_set_request(msg, "application/json", SOUP_MEMORY_TAKE, string, strlen(string));
    soup_session_queue_message(session, msg, _eventd_webhook_message_callback, NULL);

    g_object_unref(session);
    g_free(url);
}


/*
 * Plugin interface
 */

EVENTD_EXPORT const gchar *eventd_plugin_id = "webhook";
EVENTD_EXPORT const gboolean eventd_plugin_system_mode_support = TRUE;
EVENTD_EXPORT
void
eventd_plugin_get_interface(EventdPluginInterface *interface)
{
    eventd_plugin_interface_add_init_callback(interface, _eventd_webhook_init);
    eventd_plugin_interface_add_uninit_callback(interface, _eventd_webhook_uninit);

    eventd_plugin_interface_add_global_parse_callback(interface, _eventd_webhook_global_parse);
    eventd_plugin_interface_add_action_parse_callback(interface, _eventd_webhook_action_parse);
    eventd_plugin_interface_add_config_reset_callback(interface, _eventd_webhook_config_reset);

    eventd_plugin_interface_add_event_action_callback(interface, _eventd_webhook_event_action);
}
