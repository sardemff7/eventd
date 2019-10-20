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

#include <glib.h>

#include <purple.h>

#include "eventd-plugin.h"
#include "libeventd-helpers-config.h"
#include "libeventd-helpers-reconnect.h"

#include "io.h"

struct _EventdPluginContext {
    GHashTable *accounts;
    GSList *actions;
    gboolean started;
};

typedef struct {
    EventdPluginContext *context;
    const gchar *name;
    PurplePluginProtocolInfo *prpl_info;
    PurpleAccount *account;
    GHashTable *convs;
    LibeventdReconnectHandler *reconnect;
    gint64 leave_timeout;
} EventdImAccount;

typedef enum {
    EVENTD_IM_CONV_STATE_NOT_READY,
    EVENTD_IM_CONV_STATE_JOINING,
    EVENTD_IM_CONV_STATE_READY,
    EVENTD_IM_CONV_STATE_ALWAYS_READY,
} EventdImConvState;

typedef struct {
    EventdImAccount *account;
    PurpleConversationType type;
    const gchar *name;
    PurpleConversation *conv;
    EventdImConvState state;
    guint leave_timeout;
    GPtrArray *pending_messages;
} EventdImConv;

struct _EventdPluginAction {
    GSList *convs;
    FormatString *message;
};

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

    g_free(account);
}

static void
_eventd_im_conv_flush(EventdImConv *conv)
{
    if ( conv->pending_messages->len == 0 )
        return;

    if ( conv->conv == NULL )
    {
        if ( ! purple_account_is_connected(conv->account->account) )
            return;

        switch ( conv->state )
        {
        case EVENTD_IM_CONV_STATE_READY:
        case EVENTD_IM_CONV_STATE_ALWAYS_READY:
            conv->conv = purple_conversation_new(conv->type, conv->account->account, conv->name);
        break;
        case EVENTD_IM_CONV_STATE_NOT_READY:
        {
            GHashTable *comps;
            comps = g_hash_table_new(g_str_hash, g_str_equal);
            g_hash_table_insert(comps, (gpointer) "channel", (gpointer) conv->name);
            conv->account->prpl_info->join_chat(purple_account_get_connection(conv->account->account), comps);
            g_hash_table_unref(comps);
            conv->state = EVENTD_IM_CONV_STATE_JOINING;
        case EVENTD_IM_CONV_STATE_JOINING:
            return;
        }
        }
    }

    guint i;
    for ( i = 0 ; i < conv->pending_messages->len ; ++i )
    {
        gchar *message = conv->pending_messages->pdata[i];
        switch ( conv->type )
        {
        case PURPLE_CONV_TYPE_IM:
            purple_conv_im_send(PURPLE_CONV_IM(conv->conv), message);
        break;
        case PURPLE_CONV_TYPE_CHAT:
            purple_conv_chat_send(PURPLE_CONV_CHAT(conv->conv), message);
        break;
        default:
        break;
        }
    }
    g_ptr_array_remove_range(conv->pending_messages, 0, conv->pending_messages->len);
}

static void
_eventd_im_conv_reset(EventdImConv *conv)
{
    if ( conv->state <= EVENTD_IM_CONV_STATE_READY )
    {
        conv->state = EVENTD_IM_CONV_STATE_NOT_READY;
        if ( conv->conv != NULL )
            purple_conversation_destroy(conv->conv);
        conv->conv = NULL;
    }

    if ( conv->leave_timeout > 0 )
        g_source_remove(conv->leave_timeout);
    conv->leave_timeout = 0;
}

static void
_eventd_im_conv_free(gpointer data)
{
    if ( data == NULL )
        return;

    EventdImConv *conv = data;

    _eventd_im_conv_reset(conv);

    g_ptr_array_free(conv->pending_messages, TRUE);

    g_slice_free(EventdImConv, conv);
}

static void
_eventd_im_action_free(gpointer data)
{
    if ( data == NULL )
        return;

    EventdPluginAction *action = data;

    g_slist_free(action->convs);
    evhelpers_format_string_unref(action->message);

    g_slice_free(EventdPluginAction, action);
}

static void
_eventd_im_account_reconnect_callback(LibeventdReconnectHandler *handler, gpointer user_data)
{
    EventdImAccount *account = user_data;

    purple_account_connect(account->account);
}

static void
_eventd_im_signed_on_callback(PurpleAccount *ac, EventdPluginContext *context)
{
    EventdImAccount *account = ac->ui_data;

    evhelpers_reconnect_reset(account->reconnect);

    GHashTableIter iter;
    EventdImConv *conv;
    g_hash_table_iter_init(&iter, account->convs);
    while ( g_hash_table_iter_next(&iter, NULL, (gpointer *) &conv) )
        _eventd_im_conv_flush(conv);
}

static void
_eventd_im_signed_off_callback(PurpleAccount *ac, EventdPluginContext *context)
{
    EventdImAccount *account = ac->ui_data;
    const PurpleConnectionErrorInfo *err;

    GHashTableIter iter;
    EventdImConv *conv;
    g_hash_table_iter_init(&iter, account->convs);
    while ( g_hash_table_iter_next(&iter, NULL, (gpointer *) &conv) )
        _eventd_im_conv_reset(conv);

    err = purple_account_get_current_error(account->account);
    if ( err == NULL )
        evhelpers_reconnect_reset(account->reconnect);
    else if ( ! purple_connection_error_is_fatal(err->type) )
    {
        if ( ! evhelpers_reconnect_try(account->reconnect) )
            g_warning("Too many reconnect tries for account %s", account->name);
    }
}

static void
_eventd_im_error_callback(PurpleAccount *ac, PurpleConnectionError err, const gchar *desc, EventdPluginContext *context)
{
    EventdImAccount *account = ac->ui_data;
    g_debug("Error on account %s: %s", account->name, desc);
}

static gboolean
_eventd_im_conv_timeout(gpointer user_data)
{
    EventdImConv *conv = user_data;

    conv->leave_timeout = 0;
    _eventd_im_conv_reset(conv);
    return G_SOURCE_REMOVE;
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

    conv->state = EVENTD_IM_CONV_STATE_READY;
    _eventd_im_conv_flush(conv);

    if ( account->leave_timeout < 0 )
        return;

    if ( conv->leave_timeout > 0 )
        g_source_remove(conv->leave_timeout);
    conv->leave_timeout = g_timeout_add_seconds(account->leave_timeout, _eventd_im_conv_timeout, conv);
}

static void
_eventd_im_conv_left(PurpleConversation *_conv, EventdPluginContext *context)
{
    EventdImAccount *account;
    EventdImConv *conv;

    account = purple_conversation_get_account(_conv)->ui_data;
    conv = g_hash_table_lookup(account->convs, purple_conversation_get_name(_conv));
    g_return_if_fail(conv != NULL);

    _eventd_im_conv_reset(conv);
}

/*
 * Initialization interface
 */

static EventdPluginContext *
_eventd_im_init(EventdPluginCoreContext *core)
{
#ifdef G_OS_UNIX
    /* Some ugly workaround for libpurple plugins that do not link to it */
    static GModule *libpurple_module = NULL;
    if ( libpurple_module == NULL )
    {
        static const gchar *_eventd_im_libpurple_so_names[] = { "libpurple.so.0", "libpurple" };
        gsize i;
        for ( i = 0 ; ( libpurple_module == NULL ) && ( i < G_N_ELEMENTS(_eventd_im_libpurple_so_names) ) ; ++i )
            libpurple_module = g_module_open(_eventd_im_libpurple_so_names[i], G_MODULE_BIND_LAZY);
        if ( libpurple_module == NULL )
        {
            g_warning("Couldn't load libpurple");
            return NULL;
        }
        g_module_make_resident(libpurple_module);
    }
#endif /* G_OS_UNIX */

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

    context->accounts = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, _eventd_im_account_free);

    purple_signal_connect(purple_accounts_get_handle(), "account-signed-on", context, PURPLE_CALLBACK(_eventd_im_signed_on_callback), context);
    purple_signal_connect(purple_accounts_get_handle(), "account-signed-off", context, PURPLE_CALLBACK(_eventd_im_signed_off_callback), context);
    purple_signal_connect(purple_accounts_get_handle(), "account-connection-error", context, PURPLE_CALLBACK(_eventd_im_error_callback), context);
    purple_signal_connect(purple_conversations_get_handle(), "chat-joined", context, PURPLE_CALLBACK(_eventd_im_conv_joined), context);
    purple_signal_connect(purple_conversations_get_handle(), "chat-left", context, PURPLE_CALLBACK(_eventd_im_conv_left), context);

    return context;
}

static void
_eventd_im_uninit(EventdPluginContext *context)
{
    g_hash_table_unref(context->accounts);

    g_free(context);
}


/*
 * Start/stop interface
 */

static void
_eventd_im_start(EventdPluginContext *context)
{
    context->started = TRUE;
    GHashTableIter iter;
    gchar *name;
    EventdImAccount *account;
    g_hash_table_iter_init(&iter, context->accounts);
    while ( g_hash_table_iter_next(&iter, (gpointer *) &name, (gpointer *) &account) )
        purple_account_set_enabled(account->account, PACKAGE_NAME, TRUE);
}

static void
_eventd_im_stop(EventdPluginContext *context)
{
    context->started = FALSE;
    GHashTableIter iter;
    gchar *name;
    EventdImAccount *account;
    g_hash_table_iter_init(&iter, context->accounts);
    while ( g_hash_table_iter_next(&iter, (gpointer *) &name, (gpointer *) &account) )
        purple_account_set_enabled(account->account, PACKAGE_NAME, FALSE);
}


/*
 * Control command interface
 */

static EventdPluginCommandStatus
_eventd_im_account_check_status(EventdImAccount *self, const gchar **s)
{
    *s = "connected";
    if ( purple_account_is_connecting(self->account) )
    {
        *s = "connecting";
        return EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_1;
    }
    else if ( purple_account_is_disconnected(self->account) )
    {
        if ( evhelpers_reconnect_too_much(self->reconnect) )
        {
            *s = "disconnected, too much reconnection attempts";
            return EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_3;
        }
        else
        {
            *s = "disconnected";
            return EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_2;
        }
    }
    return EVENTD_PLUGIN_COMMAND_STATUS_OK;
}

static EventdPluginCommandStatus
_eventd_im_control_command(EventdPluginContext *context, guint64 argc, const gchar * const *argv, gchar **status)
{
    EventdImAccount *account;
    EventdPluginCommandStatus r = EVENTD_PLUGIN_COMMAND_STATUS_OK;

    if ( g_strcmp0(argv[0], "connect") == 0 )
    {
        if ( argc < 2 )
        {
            *status = g_strdup("No account specified");
            r = EVENTD_PLUGIN_COMMAND_STATUS_COMMAND_ERROR;
        }
        else if ( ( account = g_hash_table_lookup(context->accounts, argv[1]) ) == NULL )
        {
            *status = g_strdup_printf("No such account '%s'", argv[1]);
            r = EVENTD_PLUGIN_COMMAND_STATUS_EXEC_ERROR;
        }
        else
        {
            evhelpers_reconnect_reset(account->reconnect);
            purple_account_connect(account->account);
            *status = g_strdup_printf("Connected to account '%s'", argv[1]);
        }
    }
    else if ( g_strcmp0(argv[0], "disconnect") == 0 )
    {
        if ( argc < 2 )
        {
            *status = g_strdup("No account specified");
            r = EVENTD_PLUGIN_COMMAND_STATUS_COMMAND_ERROR;
        }
        else if ( ( account = g_hash_table_lookup(context->accounts, argv[1]) ) == NULL )
        {
            *status = g_strdup_printf("No such account '%s'", argv[1]);
            r = EVENTD_PLUGIN_COMMAND_STATUS_EXEC_ERROR;
        }
        else
        {
            if ( ! purple_account_is_disconnected(account->account) )
                purple_account_disconnect(account->account);
            *status = g_strdup_printf("Disconnected from account '%s'", argv[1]);
        }
    }
    else if ( g_strcmp0(argv[0], "status") == 0 )
    {
        const gchar *s;
        if ( argc < 2 )
        {
            if ( g_hash_table_size(context->accounts) == 0 )
            {
                r = EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_1;
                *status = g_strdup("No account");
            }
            else
            {
                GString *list;
                list = g_string_sized_new(strlen("Accounts list:") + strlen("\n    a quit long name") * g_hash_table_size(context->accounts));
                g_string_append(list, "Accounts list:");

                GHashTableIter iter;
                const gchar *name;
                g_hash_table_iter_init(&iter, context->accounts);
                while ( g_hash_table_iter_next(&iter, (gpointer *) &name, (gpointer *) &account) )
                {
                    _eventd_im_account_check_status(account, &s);
                    g_string_append_printf(list, "\n    %s: %s", name, s);
                }

                *status = g_string_free(list, FALSE);
            }
        }
        else if ( ( account = g_hash_table_lookup(context->accounts, argv[1]) ) == NULL )
        {
            *status = g_strdup_printf("No such account '%s'", argv[1]);
            r = EVENTD_PLUGIN_COMMAND_STATUS_EXEC_ERROR;
        }
        else
        {
            r = _eventd_im_account_check_status(account, &s);
            *status = g_strdup_printf("Account '%s' is %s", argv[1], s);
        }
        GHashTableIter iter;
        g_hash_table_iter_init(&iter, context->accounts);
    }
    else if ( g_strcmp0(argv[0], "list") == 0 )
    {
        if ( g_hash_table_size(context->accounts) == 0 )
        {
            r = EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_1;
            *status = g_strdup("No account");
        }
        else
        {
            GString *list;
            list = g_string_sized_new(strlen("Accounts list:") + strlen("\n    a quit long name") * g_hash_table_size(context->accounts));
            g_string_append(list, "Accounts list:");
            GHashTableIter iter;
            const gchar *name;
            g_hash_table_iter_init(&iter, context->accounts);
            while ( g_hash_table_iter_next(&iter, (gpointer *) &name, NULL) )
                g_string_append(g_string_append(list, "\n    "), name);

            *status = g_string_free(list, FALSE);
        }
    }
    else
    {
        *status = g_strdup_printf("Unknown command '%s'", argv[0]);
        r = EVENTD_PLUGIN_COMMAND_STATUS_COMMAND_ERROR;
    }

    return r;
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
    if ( names == NULL )
        return;

    gchar **name, *section;
    for ( name = names ; *name != NULL ; ++name )
    {
        gchar *protocol = NULL;
        gchar *username = NULL;
        gchar *password = NULL;
        Int port;
        gboolean use_tls = TRUE;
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
        if ( evhelpers_config_key_file_get_boolean(config_file, section, "UseTLS", &use_tls) < 0 )
            goto next;
        if ( evhelpers_config_key_file_get_int_with_default(config_file, section, "ReconnectTimeout", 5, &reconnect_timeout) < 0 )
            goto next;
        if ( evhelpers_config_key_file_get_int_with_default(config_file, section, "ReconnectMaxTries", 0, &reconnect_max_tries) < 0 )
            goto next;
        if ( evhelpers_config_key_file_get_int_with_default(config_file, section, "ChatLeaveTimeout", -1, &leave_timeout) < 0 )
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

        account->prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);
        account->account = purple_account_new(username, protocol);
        account->account->ui_data = account;
        if ( password != NULL )
            purple_account_set_password(account->account, password);
        purple_accounts_add(account->account);
        purple_account_set_alias(account->account, account->name);

        if ( port.set )
            purple_account_set_int(account->account, "port", CLAMP(port.value, 1, G_MAXUINT16));
        if ( use_tls )
        {
            purple_account_set_bool(account->account, "ssl", TRUE);
            purple_account_set_string(account->account, "connection_security", "require_tls");
            purple_account_set_string(account->account, "encryption", "require_encryption");
        }

        account->convs = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, _eventd_im_conv_free);

        account->reconnect = evhelpers_reconnect_new(reconnect_timeout, reconnect_max_tries, _eventd_im_account_reconnect_callback, account);

        account->leave_timeout = leave_timeout;

        g_hash_table_insert(context->accounts, *name, account);
        *name = NULL;

    next:
        g_free(password);
        g_free(username);
        g_free(protocol);
        g_free(section);
        g_free(*name);
    }
    g_free(names);
}

static EventdPluginAction *
_eventd_im_action_parse(EventdPluginContext *context, GKeyFile *config_file)
{
    if ( ! g_key_file_has_group(config_file, "IM") )
        return NULL;

    gboolean disable = FALSE;
    if ( evhelpers_config_key_file_get_boolean(config_file, "IM", "Disable", &disable) < 0 )
        return NULL;

    if ( disable )
        return NULL;

    FormatString *message = NULL;
    gchar *account_name = NULL;
    gchar **recipients = NULL, **recipient;
    gboolean chat = TRUE;

    if ( evhelpers_config_key_file_get_locale_format_string(config_file, "IM", "Message", NULL, &message) != 0 )
        goto error;

    if ( evhelpers_config_key_file_get_string(config_file, "IM", "Account", &account_name) != 0 )
        goto error;

    if ( evhelpers_config_key_file_get_string_list(config_file, "IM", "Recipients", &recipients, NULL) != 0 )
        goto error;
    if ( evhelpers_config_key_file_get_boolean(config_file, "IM", "Chat", &chat) < 0 )
        goto error;

    EventdImAccount *account;
    account = g_hash_table_lookup(context->accounts, account_name);
    if ( account == NULL )
        goto error;
    g_free(account_name);

    GSList *convs = NULL;
    for ( recipient = recipients ; *recipient != NULL ; ++recipient )
    {
        EventdImConv *conv;
        conv = g_hash_table_lookup(account->convs, *recipient);
        if ( conv == NULL )
        {
            conv = g_slice_new0(EventdImConv);
            conv->account = account;
            conv->type = chat ? PURPLE_CONV_TYPE_CHAT : PURPLE_CONV_TYPE_IM;
            conv->name = *recipient;
            conv->state = ( chat && ( account->prpl_info->join_chat != NULL ) ) ? EVENTD_IM_CONV_STATE_NOT_READY : EVENTD_IM_CONV_STATE_ALWAYS_READY;
            conv->pending_messages = g_ptr_array_new_with_free_func(g_free);
            g_hash_table_insert(account->convs, *recipient, conv);
        }
        else
            g_free(*recipient);
        convs = g_slist_prepend(convs, conv);
    }
    g_free(recipients);

    EventdPluginAction *action;
    action = g_slice_new(EventdPluginAction);
    action->convs = convs;
    action->message = message;

    return action;

error:
    g_strfreev(recipients);
    g_free(account_name);
    evhelpers_format_string_unref(message);
    return NULL;
}

static void
_eventd_im_config_reset(EventdPluginContext *context)
{
    g_slist_free_full(context->actions, _eventd_im_action_free);
    context->actions = NULL;
    g_hash_table_remove_all(context->accounts);
}


/*
 * Event action interface
 */

static void
_eventd_im_event_action(EventdPluginContext *context, EventdPluginAction *action, EventdEvent *event)
{
    gchar *message;

    message = evhelpers_format_string_get_string(action->message, event, NULL, NULL);

    GSList *conv_;
    for ( conv_ = action->convs ; conv_ != NULL ; conv_ = g_slist_next(conv_) )
    {
        EventdImConv *conv = conv_->data;
        g_ptr_array_add(conv->pending_messages, g_strdup(message));
        _eventd_im_conv_flush(conv);
    }

    g_free(message);
}


/*
 * Plugin interface
 */

EVENTD_EXPORT const gchar *eventd_plugin_id = "im";
EVENTD_EXPORT const gboolean eventd_plugin_system_mode_support = TRUE;
EVENTD_EXPORT
void
eventd_plugin_get_interface(EventdPluginInterface *interface)
{
    eventd_plugin_interface_add_init_callback(interface, _eventd_im_init);
    eventd_plugin_interface_add_uninit_callback(interface, _eventd_im_uninit);

    eventd_plugin_interface_add_start_callback(interface, _eventd_im_start);
    eventd_plugin_interface_add_stop_callback(interface, _eventd_im_stop);

    eventd_plugin_interface_add_control_command_callback(interface, _eventd_im_control_command);

    eventd_plugin_interface_add_global_parse_callback(interface, _eventd_im_global_parse);
    eventd_plugin_interface_add_action_parse_callback(interface, _eventd_im_action_parse);
    eventd_plugin_interface_add_config_reset_callback(interface, _eventd_im_config_reset);

    eventd_plugin_interface_add_event_action_callback(interface, _eventd_im_event_action);
}
