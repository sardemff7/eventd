/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2015 Quentin "Sardem FF7" Glidic
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

#include <glib.h>

#include <purple.h>

#include <eventd-plugin.h>
#include <libeventd-helpers-config.h>
#include <libeventd-helpers-reconnect.h>

#include "io.h"

struct _EventdPluginContext {
    GList *accounts;
    GHashTable *events;
};

typedef struct {
    EventdPluginContext *context;
    gchar *name;
    PurplePluginProtocolInfo *prpl_info;
    PurpleAccount *account;
    GHashTable *convs;
    LibeventdReconnectHandler *reconnect;
    gint64 leave_timeout;
    gboolean started;
} EventdImAccount;

typedef struct {
    EventdImAccount *account;
    FormatString *message;
    GList *convs;
} EventdImEventAccount;

typedef struct {
    PurpleConversationType type;
    const gchar *channel;
    PurpleConversation *conv;
    guint leave_timeout;
    GList *pending_messages;
} EventdImConv;

static PurpleEventLoopUiOps
_eventd_im_ui_ops = {
    .timeout_add         = g_timeout_add,
    .timeout_remove      = g_source_remove,
    .input_add           = eventd_im_glib_input_add,
    .input_remove        = g_source_remove,
    .input_get_error     = NULL,
    .timeout_add_seconds = g_timeout_add_seconds,
};


static void
_eventd_im_account_free(gpointer data)
{
    EventdImAccount *account = data;

    purple_account_destroy(account->account);
    g_free(account->name);

    g_free(account);
}

static void
_eventd_im_account_connect(EventdImAccount *account)
{
    if ( ! account->started )
        return;

    if ( purple_account_is_connected(account->account) || purple_account_is_connecting(account->account) )
        return;

    purple_account_connect(account->account);
}

static void
_eventd_im_conv_free(gpointer data)
{
    if ( data == NULL )
        return;

    EventdImConv *conv = data;

    if ( conv->conv != NULL )
        purple_conversation_destroy(conv->conv);

    if ( conv->leave_timeout > 0 )
        g_source_remove(conv->leave_timeout);

    g_list_free_full(conv->pending_messages, g_free);

    g_slice_free(EventdImConv, conv);
}

static void
_eventd_im_event_account_free(gpointer data)
{
    if ( data == NULL )
        return;

    EventdImEventAccount *account = data;

    g_list_free_full(account->convs, _eventd_im_conv_free);
    evhelpers_format_string_unref(account->message);

    g_slice_free(EventdImEventAccount, account);
}

static void
_eventd_im_event_free(gpointer data)
{
    g_list_free_full(data, _eventd_im_event_account_free);
}

static void
_eventd_im_account_reconnect_callback(LibeventdReconnectHandler *handler, gpointer user_data)
{
    EventdImAccount *account = user_data;

    _eventd_im_account_connect(account);
}

static void
_eventd_im_signed_on_callback(PurpleConnection *gc, EventdPluginContext *context)
{
    EventdImAccount *account = purple_connection_get_account(gc)->ui_data;

    evhelpers_reconnect_reset(account->reconnect);
}

static void
_eventd_im_error_callback(PurpleAccount *ac, const PurpleConnectionErrorInfo *old_error, const PurpleConnectionErrorInfo *current_error, EventdPluginContext *context)
{
    EventdImAccount *account = ac->ui_data;
    g_debug("Error on account %s: %s", account->name, current_error->description);
    if ( account->started && ( ! purple_account_is_connecting(account->account) ) )
    {
        if ( ! evhelpers_reconnect_try(account->reconnect) )
            g_warning("Too many reconnect tries for account %s", account->name);
    }
}

static gboolean
_eventd_im_conv_timeout(gpointer user_data)
{
    EventdImConv *conv = user_data;

    purple_conversation_destroy(conv->conv);
    conv->conv = NULL;
    conv->leave_timeout = 0;
    return FALSE;
}

static void
_eventd_im_conv_joined(PurpleConversation *_conv, EventdPluginContext *context)
{
    EventdImAccount *account;
    EventdImConv *conv;

    account = purple_conversation_get_account(_conv)->ui_data;
    conv = g_hash_table_lookup(account->convs, purple_conversation_get_name(_conv));
    g_return_if_fail(conv != NULL);

    conv->conv = _conv;

    GList *msg;
    for ( msg = conv->pending_messages ; msg != NULL ; msg = g_list_next(msg) )
        purple_conv_chat_send(PURPLE_CONV_CHAT(conv->conv), msg->data);
    g_list_free_full(conv->pending_messages, g_free);
    conv->pending_messages = NULL;

    if ( account->leave_timeout < 0 )
        return;

    if ( conv->leave_timeout > 0 )
        g_source_remove(conv->leave_timeout);
    conv->leave_timeout = g_timeout_add_seconds(account->leave_timeout, _eventd_im_conv_timeout, conv);
}

/*
 * Initialization interface
 */

static EventdPluginContext *
_eventd_im_init(EventdPluginCoreContext *core, EventdPluginCoreInterface *interface)
{
#ifdef PURPLE_NEEDS_GLOBAL_LOADING
    /* Some ugly workaround for libpurple plugins that do not link to it */
    static GModule *libpurple_module = NULL;
    if ( libpurple_module == NULL )
    {
        libpurple_module = g_module_open("libpurple", G_MODULE_BIND_LAZY);
        if ( libpurple_module == NULL )
        {
            g_warning("Couldn't load libpurple");
            return NULL;
        }
        g_module_make_resident(libpurple_module);
    }
#endif

    purple_util_set_user_dir("/dev/null");

    purple_debug_set_enabled(FALSE);

    purple_eventloop_set_ui_ops(&_eventd_im_ui_ops);

    if ( ! purple_core_init(PACKAGE_NAME) )
        return NULL;

    purple_set_blist(purple_blist_new());
    purple_blist_load();

    purple_prefs_load();
    purple_prefs_set_bool("/purple/away/away_when_idle", FALSE);

    purple_pounces_load();

    EventdPluginContext *context;

    context = g_new0(EventdPluginContext, 1);

    context->events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, _eventd_im_event_free);

    purple_signal_connect(purple_connections_get_handle(), "signed-on", context, PURPLE_CALLBACK(_eventd_im_signed_on_callback), context);
    purple_signal_connect(purple_connections_get_handle(), "error-changed", context, PURPLE_CALLBACK(_eventd_im_error_callback), context);
    purple_signal_connect(purple_conversations_get_handle(), "chat-joined", context, PURPLE_CALLBACK(_eventd_im_conv_joined), context);

    return context;
}

static void
_eventd_im_uninit(EventdPluginContext *context)
{
    g_list_free_full(context->accounts, _eventd_im_account_free);
    g_hash_table_unref(context->events);

    g_free(context);
}


/*
 * Configuration interface
 */
static void
_eventd_im_global_parse(EventdPluginContext *context, GKeyFile *config_file)
{
    if ( ! g_key_file_has_group(config_file, "IM") )
        return;

    gchar **names;
    if ( evhelpers_config_key_file_get_string_list(config_file, "IM", "Accounts", &names, NULL) < 0 )
        return;

    gchar **name, *section;
    for ( name = names ; *name != NULL ; ++name )
    {
        gchar *protocol = NULL;
        gchar *username = NULL;
        gchar *password = NULL;
        Int port;
        gint64 reconnect_timeout;
        gint64 reconnect_max_tries;
        gint64 leave_timeout;
        PurplePlugin *prpl;

        section = g_strconcat("IMAccount ", *name, NULL);
        if ( ! g_key_file_has_group(config_file, section) )
            goto next;

        if ( evhelpers_config_key_file_get_string(config_file, section, "Protocol", &protocol) < 0 )
            goto next;
        if ( evhelpers_config_key_file_get_string(config_file, section, "Username", &username) < 0 )
            goto next;
        if ( evhelpers_config_key_file_get_string(config_file, section, "Password", &password) < 0 )
            goto next;
        if ( evhelpers_config_key_file_get_int(config_file, section, "Port", &port) < 0 )
            goto next;
        if ( evhelpers_config_key_file_get_int_with_default(config_file, section, "ReconnectTimeout", 5, &reconnect_timeout) < 0 )
            goto next;
        if ( evhelpers_config_key_file_get_int_with_default(config_file, section, "ReconnectMaxTries", 10, &reconnect_max_tries) < 0 )
            goto next;
        if ( evhelpers_config_key_file_get_int_with_default(config_file, section, "ChatLeaveTimeout", 1200, &leave_timeout) < 0 )
            goto next;

        prpl = purple_find_prpl(protocol);
        if ( prpl == NULL )
        {
            g_warning("Unknown protocol %s", protocol);
            goto next;
        }

        EventdImAccount *account;
        account = g_new0(EventdImAccount, 1);
        account->context = context;
        account->name = *name;
        *name = NULL;

        account->prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);
        account->account = purple_account_new(username, protocol);
        account->account->ui_data = account;
        if ( password != NULL )
            purple_account_set_password(account->account, password);
        purple_accounts_add(account->account);
        purple_account_set_alias(account->account, account->name);
        purple_account_set_enabled(account->account, PACKAGE_NAME, TRUE);

        if ( port.set )
            purple_account_set_int(account->account, "port", CLAMP(port.value, 1, G_MAXUINT16));

        account->convs = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) purple_conversation_destroy);

        account->reconnect = evhelpers_reconnect_new(reconnect_timeout, reconnect_max_tries, _eventd_im_account_reconnect_callback, account);

        account->leave_timeout = leave_timeout;

        context->accounts = g_list_prepend(context->accounts, account);

    next:
        g_free(password);
        g_free(username);
        g_free(protocol);
        g_free(section);
        g_free(*name);
    }
    g_free(names);
}

static void
_eventd_im_event_parse(EventdPluginContext *context, const gchar *config_id, GKeyFile *config_file)
{
    if ( g_key_file_has_group(config_file, "IM") )
    {
        gboolean disable;
        if ( evhelpers_config_key_file_get_boolean(config_file, "IM", "Disable", &disable) < 0 )
            return;

        if ( disable )
        {
            g_hash_table_insert(context->events, g_strdup(config_id), NULL);
            return;
        }
    }

    gboolean have_account = FALSE;
    GList *event_accounts = NULL;

    GList *account_;
    gchar *section;
    for ( account_ = context->accounts ; account_ != NULL ; account_ = g_list_next(account_) )
    {
        EventdImAccount *account = account_->data;

        section = g_strconcat("IMAccount ", account->name, NULL);
        if ( ! g_key_file_has_group(config_file, section) )
            goto next;

        have_account = TRUE;

        FormatString *message = NULL;
        if ( evhelpers_config_key_file_get_locale_format_string(config_file, section, "Message", NULL, &message) != 0 )
            goto next;

        gchar **channels;
        evhelpers_config_key_file_get_string_list(config_file, section, "Channels", &channels, NULL);

        /*
         * TODO: private messages
         */

        if ( channels == NULL )
        {
            g_free(message);
            goto next;
        }

        EventdImEventAccount *event_account;
        event_account = g_slice_new0(EventdImEventAccount);
        event_account->account = account;
        event_account->message = message;

        if ( channels != NULL )
        {
            gchar **channel;
            for ( channel = channels ; *channel != NULL ; ++channel )
            {
                EventdImConv *conv;
                conv = g_hash_table_lookup(account->convs, *channel);
                if ( conv == NULL )
                {
                    conv = g_slice_new0(EventdImConv);
                    conv->type = PURPLE_CONV_TYPE_CHAT;
                    conv->channel = *channel;
                    if ( account->prpl_info->join_chat == NULL )
                        conv->conv = purple_conversation_new(PURPLE_CONV_TYPE_CHAT, account->account, *channel);
                    g_hash_table_insert(account->convs, *channel, conv);
                }
                event_account->convs = g_list_prepend(event_account->convs, conv);
            }
            g_free(channels);
        }

        event_accounts = g_list_prepend(event_accounts, event_account);

    next:
        g_free(section);
    }

    if ( have_account )
        g_hash_table_insert(context->events, g_strdup(config_id), event_accounts);
}

static void
_eventd_im_config_reset(EventdPluginContext *context)
{
    g_hash_table_remove_all(context->events);
    g_list_free_full(context->accounts, _eventd_im_account_free);
    context->accounts = NULL;
}

static void
_eventd_im_start(EventdPluginContext *context)
{
    GList *account_;
    for ( account_ = context->accounts ; account_ != NULL ; account_ = g_list_next(account_) )
    {
        EventdImAccount *account = account_->data;
        account->started = TRUE;
        _eventd_im_account_connect(account);
    }
}

static void
_eventd_im_stop(EventdPluginContext *context)
{
    GList *account_;
    for ( account_ = context->accounts ; account_ != NULL ; account_ = g_list_next(account_) )
    {
        EventdImAccount *account = account_->data;
        account->started = FALSE;
        evhelpers_reconnect_reset(account->reconnect);
        purple_account_disconnect(account->account);
    }
}


/*
 * Event action interface
 */

static void
_eventd_im_event_action(EventdPluginContext *context, const gchar *config_id, EventdEvent *event)
{
    GList *accounts;

    accounts = g_hash_table_lookup(context->events, config_id);
    if ( accounts == NULL )
        return;

    GHashTable *comps;
    comps = g_hash_table_new(g_str_hash, g_str_equal);

    GList *account_;
    EventdImEventAccount *account;
    for ( account_ = accounts ; account_ != NULL ; account_ = g_list_next(account_) )
    {
        account = account_->data;

        if ( ! purple_account_is_connected(account->account->account) )
            continue;

        PurpleConnection *gc;
        gchar *message;

        gc = purple_account_get_connection(account->account->account);
        message = evhelpers_format_string_get_string(account->message, event, NULL, NULL);

        GList *conv_;
        for ( conv_ = account->convs ; conv_ != NULL ; conv_ = g_list_next(conv_) )
        {
            EventdImConv *conv = conv_->data;

            switch ( conv->type )
            {
            case PURPLE_CONV_TYPE_CHAT:
                if ( conv->conv != NULL )
                {
                    purple_conv_chat_send(PURPLE_CONV_CHAT(conv->conv), message);
                    break;
                }

                if ( conv->pending_messages == NULL )
                {
                    g_hash_table_replace(comps, (gpointer) "channel", (gpointer) conv->channel);
                    account->account->prpl_info->join_chat(gc, comps);
                }
                conv->pending_messages = g_list_prepend(conv->pending_messages, g_strdup(message));
            break;
            default:
            break;
            }
        }

        g_free(message);
    }

    g_hash_table_unref(comps);
}


/*
 * Plugin interface
 */

EVENTD_EXPORT const gchar *eventd_plugin_id = "im";
EVENTD_EXPORT
void
eventd_plugin_get_interface(EventdPluginInterface *interface)
{
    eventd_plugin_interface_add_init_callback(interface, _eventd_im_init);
    eventd_plugin_interface_add_uninit_callback(interface, _eventd_im_uninit);

    eventd_plugin_interface_add_global_parse_callback(interface, _eventd_im_global_parse);
    eventd_plugin_interface_add_event_parse_callback(interface, _eventd_im_event_parse);
    eventd_plugin_interface_add_config_reset_callback(interface, _eventd_im_config_reset);

    eventd_plugin_interface_add_start_callback(interface, _eventd_im_start);
    eventd_plugin_interface_add_stop_callback(interface, _eventd_im_stop);

    eventd_plugin_interface_add_event_action_callback(interface, _eventd_im_event_action);
}
