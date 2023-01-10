/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2021 Quentin "Sardem FF7" Glidic
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

typedef enum {
    EVENTD_WEBHOOK_METHOD_POST,
    EVENTD_WEBHOOK_METHOD_PUT,
    EVENTD_WEBHOOK_METHOD_GET,
} EventdWebhookMethod;

struct _EventdPluginAction {
    EventdWebhookMethod method;
    FormatString *url;
    FormatString *string;
    gchar *content_type;
    GHashTable *headers;
};

static const gchar * const _eventd_webhook_method[] = {
    [EVENTD_WEBHOOK_METHOD_POST] = "POST",
    [EVENTD_WEBHOOK_METHOD_PUT]  = "PUT",
    [EVENTD_WEBHOOK_METHOD_GET]  = "GET",
};

static void
_eventd_webhook_action_free(gpointer data)
{
    EventdPluginAction *action = data;

    if ( action->headers != NULL )
        g_hash_table_unref(action->headers);
    g_free(action->content_type);
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
    gchar *content_type = NULL;
    guint64 method;
    GHashTable *headers = NULL;

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

    if ( evhelpers_config_key_file_get_string_with_default(config_file, "WebHook", "ContentType", "application/json", &content_type) < 0 )
        goto fail;

    if ( evhelpers_config_key_file_get_enum_with_default(config_file, "WebHook", "Method", _eventd_webhook_method, G_N_ELEMENTS(_eventd_webhook_method), EVENTD_WEBHOOK_METHOD_POST, &method) < 0 )
        goto fail;

    if ( g_key_file_has_group(config_file, "WebHook Headers") )
    {
        gchar **keys, **key;

        headers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        keys = g_key_file_get_keys(config_file, "WebHook Headers", NULL, NULL);
        for ( key = keys ; *key != NULL ; ++key )
        {
            gchar *value;
            value = g_key_file_get_string(config_file, "WebHook Headers", *key, NULL);
            g_hash_table_insert(headers, *key, value);
        }
        g_free(keys);
    }

    EventdPluginAction *action;
    action = g_slice_new(EventdPluginAction);
    action->method = method;
    action->url = url;
    action->string = string;
    action->content_type = content_type;
    action->headers = headers;

    context->actions = g_slist_prepend(context->actions, action);

    return action;

fail:
    g_free(content_type);
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
_eventd_webhook_message_callback(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    GBytes *bytes;

    SoupMessage *msg = soup_session_get_async_result_message(SOUP_SESSION(obj), res);
    GUri *uri = soup_message_get_uri(msg);

    bytes = soup_session_send_and_read_finish(SOUP_SESSION(obj), res, &error);
    if ( bytes == NULL )
    {
        g_warning("Could not send message to %s://%s%s%s%s: %s",
            g_uri_get_scheme(uri),
            g_uri_get_host(uri),
            g_uri_get_path(uri),
            g_uri_get_query(uri),
            g_uri_get_fragment(uri),
            error->message);
        g_clear_error(&error);
        return;
    }
    g_bytes_unref(bytes);

    SoupStatus status = soup_message_get_status(msg);
    const gchar *success = "not successful";
    GLogLevelFlags log_level = G_LOG_LEVEL_WARNING;
    if ( SOUP_STATUS_IS_SUCCESSFUL(status) )
    {
        success = "successful";
        log_level = G_LOG_LEVEL_INFO;
    }

    g_log(G_LOG_DOMAIN, log_level, "Message to %s://%s%s%s%s is %s: (%d %s) %s",
        g_uri_get_scheme(uri),
        g_uri_get_host(uri),
        g_uri_get_path(uri),
        g_uri_get_query(uri),
        g_uri_get_fragment(uri),
        success,
        soup_message_get_status(msg),
        soup_status_get_phrase(soup_message_get_status(msg)),
        soup_message_get_reason_phrase(msg)
    );
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
        soup_session_set_user_agent(session, PACKAGE_NAME " " NK_PACKAGE_VERSION);
    msg = soup_message_new(_eventd_webhook_method[action->method], url);
    if ( action->headers != NULL )
    {
        SoupMessageHeaders *headers;
        GHashTableIter iter;
        const gchar *header, *value;
        headers = soup_message_get_request_headers(msg);
        soup_message_headers_clear(headers);
        g_hash_table_iter_init(&iter, action->headers);
        while ( g_hash_table_iter_next(&iter, (gpointer *) &header, (gpointer *) &value) )
            soup_message_headers_append(headers, header, value);
    }
    gssize size = strlen(string);
    GInputStream *stream;
    stream = g_memory_input_stream_new_from_data(string, size, g_free);
    soup_message_set_request_body(msg, action->content_type, stream, size);
    g_object_unref(stream);
    soup_session_send_and_read_async(session, msg, G_PRIORITY_DEFAULT, NULL, _eventd_webhook_message_callback, context);
    g_object_unref(msg);

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
