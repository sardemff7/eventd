/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2016 Quentin "Sardem FF7" Glidic
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

typedef struct {
    EventdImAccount *account;
    PurpleConversationType type;
    const gchar *name;
    PurpleConversation *conv;
    guint leave_timeout;
    GList *pending_messages;
} EventdImConv;

struct _EventdPluginAction {
    EventdImConv *conv;
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
_eventd_im_account_connect(EventdImAccount *account, gboolean force)
{
    if ( ! account->context->started )
        return;

    if ( ! purple_account_is_disconnected(account->account) )
        return;

    if ( force )
        evhelpers_reconnect_reset(account->reconnect);

    purple_account_connect(account->account);
}

static void
_eventd_im_account_disconnect(EventdImAccount *account)
{
    evhelpers_reconnect_reset(account->reconnect);
    if ( purple_account_is_disconnected(account->account) )
        return;

    purple_account_disconnect(account->account);
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
_eventd_im_action_free(gpointer data)
{
    if ( data == NULL )
        return;

    EventdPluginAction *action = data;

    evhelpers_format_string_unref(action->message);

    g_slice_free(EventdPluginAction, action);
}

static void
_eventd_im_account_reconnect_callback(LibeventdReconnectHandler *handler, gpointer user_data)
{
    EventdImAccount *account = user_data;

    _eventd_im_account_connect(account, FALSE);
}

static void
_eventd_im_signed_on_callback(PurpleAccount *ac, EventdPluginContext *context)
{
    EventdImAccount *account = ac->ui_data;

    evhelpers_reconnect_reset(account->reconnect);
}

static void
_eventd_im_signed_off_callback(PurpleAccount *ac, EventdPluginContext *context)
{
    EventdImAccount *account = ac->ui_data;

    if ( purple_account_get_current_error(account->account) == NULL )
        evhelpers_reconnect_reset(account->reconnect);
    else if ( ! evhelpers_reconnect_try(account->reconnect) )
        g_warning("Too many reconnect tries for account %s", account->name);
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
        _eventd_im_account_connect(account, TRUE);
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
        _eventd_im_account_disconnect(account);
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
            _eventd_im_account_connect(account, TRUE);
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
            _eventd_im_account_disconnect(account);
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

    gboolean disable;
    if ( evhelpers_config_key_file_get_boolean(config_file, "IM", "Disable", &disable) < 0 )
        return NULL;

    if ( disable )
        return NULL;

    FormatString *message = NULL;
    gchar *account_name = NULL;
    gchar *room = NULL;

    if ( evhelpers_config_key_file_get_locale_format_string(config_file, "IM", "Message", NULL, &message) != 0 )
        goto error;

    if ( evhelpers_config_key_file_get_string(config_file, "IM", "Account", &account_name) != 0 )
        goto error;

    /*
     * TODO: private messages
     */
    if ( evhelpers_config_key_file_get_string(config_file, "IM", "Room", &room) != 0 )
        goto error;

    EventdImAccount *account;
    account = g_hash_table_lookup(context->accounts, account_name);
    if ( account == NULL )
        goto error;
    g_free(account_name);

    EventdImConv *conv;
    conv = g_hash_table_lookup(account->convs, room);
    if ( conv == NULL )
    {
        conv = g_slice_new0(EventdImConv);
        conv->account = account;
        conv->type = PURPLE_CONV_TYPE_CHAT;
        conv->name = room;
        if ( account->prpl_info->join_chat == NULL )
            conv->conv = purple_conversation_new(PURPLE_CONV_TYPE_CHAT, account->account, room);
        g_hash_table_insert(account->convs, room, conv);
        room = NULL;
    }

    EventdPluginAction *action;
    action = g_slice_new(EventdPluginAction);
    action->conv = conv;
    action->message = message;

    return action;

error:
    g_free(room);
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
    EventdImConv *conv = action->conv;
    EventdImAccount *account = conv->account;

    if ( ! purple_account_is_connected(account->account) )
        return _eventd_im_account_connect(account, FALSE);

    PurpleConnection *gc;
    gchar *message;

    gc = purple_account_get_connection(account->account);
    message = evhelpers_format_string_get_string(action->message, event, NULL, NULL);

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
            GHashTable *comps;
            comps = g_hash_table_new(g_str_hash, g_str_equal);
            g_hash_table_insert(comps, (gpointer) "channel", (gpointer) conv->name);
            account->prpl_info->join_chat(gc, comps);
            g_hash_table_unref(comps);
        }
        conv->pending_messages = g_list_prepend(conv->pending_messages, g_strdup(message));
    break;
    default:
    break;
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
