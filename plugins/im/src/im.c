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

#include <glib.h>

#include <purple.h>

#include <eventd-plugin.h>
#include <libeventd-config.h>
#include <libeventd-regex.h>

#define PURPLE_GLIB_READ_COND  (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define PURPLE_GLIB_WRITE_COND (G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL)

struct _EventdPluginContext {
    GHashTable *accounts;
    GHashTable *events;
};

typedef struct {
    PurpleInputFunction function;
    guint result;
    gpointer user_data;
} EventdImGLibIOData;

typedef struct {
    EventdPluginContext *context;
    PurpleAccount *account;
    GHashTable *convs;
} EventdImAccount;

typedef struct {
    gchar *message;
    GList *convs;
} EventdImEventAccount;

static void
_eventd_im_glib_io_free(gpointer data)
{
    g_slice_free(EventdImGLibIOData, data);
}

static gboolean
_eventd_im_glib_io_invoke(GIOChannel *source, GIOCondition cond, gpointer user_data)
{
    EventdImGLibIOData *data = user_data;
    PurpleInputCondition condition = 0;

    if ( cond & PURPLE_GLIB_READ_COND )
        condition |= PURPLE_INPUT_READ;
    if ( condition & PURPLE_GLIB_WRITE_COND )
        cond |= PURPLE_INPUT_WRITE;

    data->function(data->user_data, g_io_channel_unix_get_fd(source), condition);

    return TRUE;
}

static guint
_eventd_im_glib_input_add(gint fd, PurpleInputCondition condition, PurpleInputFunction function, gpointer user_data)
{
    EventdImGLibIOData *data;
    GIOChannel *channel;
    GIOCondition cond = 0;

    data = g_slice_new(EventdImGLibIOData);

    data->function = function;
    data->user_data = user_data;

    if ( condition & PURPLE_INPUT_READ )
        cond |= PURPLE_GLIB_READ_COND;
    if ( condition & PURPLE_INPUT_WRITE )
        cond |= PURPLE_GLIB_WRITE_COND;

    channel = g_io_channel_unix_new(fd);
    data->result = g_io_add_watch_full(channel, G_PRIORITY_DEFAULT, cond, _eventd_im_glib_io_invoke, data, _eventd_im_glib_io_free);
    g_io_channel_unref(channel);

    return data->result;
}

static PurpleEventLoopUiOps
_eventd_im_ui_ops = {
    .timeout_add         = g_timeout_add,
    .timeout_remove      = g_source_remove,
    .input_add           = _eventd_im_glib_input_add,
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
_eventd_im_event_account_free(gpointer data)
{
    if ( data == NULL )
        return;

    EventdImEventAccount *account = data;

    g_list_free(account->convs);
    g_free(account->message);

    g_slice_free(EventdImEventAccount, account);
}

static void
_eventd_im_event_free(gpointer data)
{
    g_list_free_full(data, _eventd_im_event_account_free);
}

/*
 * Initialization interface
 */

static void
_eventd_im_signed_on_callback(PurpleConnection *gc, EventdPluginContext *context)
{
    PurplePluginProtocolInfo *prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl(gc));
    if ( prpl_info->join_chat == NULL )
        return;

    EventdImAccount *account;
    account = g_hash_table_lookup(context->accounts, purple_account_get_alias(purple_connection_get_account(gc)));
    g_return_if_fail(account != NULL);

    GHashTable *comps;
    comps = g_hash_table_new(g_str_hash, g_str_equal);

    GHashTableIter iter;
    PurpleConversation *conv;
    g_hash_table_iter_init(&iter, account->convs);
    while ( g_hash_table_iter_next(&iter, NULL, (gpointer *)&conv) )
    {
        g_hash_table_replace(comps, (gpointer) "channel", (gpointer) purple_conversation_get_name(conv));
        prpl_info->join_chat(gc, comps);
    }

    g_hash_table_unref(comps);
}

static void
_eventd_im_on_error(PurpleAccount *ac, const PurpleConnectionErrorInfo *old_error, const PurpleConnectionErrorInfo *current_error, EventdPluginContext *context)
{
    g_debug("Error on account %s: %s", purple_account_get_username(ac), current_error->description);
    /*
     * TODO: Reconnect
     */
}

static EventdPluginContext *
_eventd_im_init(EventdCoreContext *core, EventdCoreInterface *interface)
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

    context->accounts = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, _eventd_im_account_free);
    context->events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, _eventd_im_event_free);

    libeventd_regex_init();

    purple_signal_connect(purple_connections_get_handle(), "signed-on", context, PURPLE_CALLBACK(_eventd_im_signed_on_callback), context);
    purple_signal_connect(purple_connections_get_handle(), "error-changed", context, PURPLE_CALLBACK(_eventd_im_on_error), context);

    return context;
}

static void
_eventd_im_uninit(EventdPluginContext *context)
{
    g_hash_table_unref(context->accounts);
    g_hash_table_unref(context->events);

    libeventd_regex_clean();

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
    if ( libeventd_config_key_file_get_string_list(config_file, "IM", "Accounts", &names, NULL) < 0 )
        return;

    gchar **name, *section;
    for ( name = names ; *name != NULL ; ++name )
    {
        section = g_strconcat("IMAccount ", *name, NULL);
        if ( ! g_key_file_has_group(config_file, section) )
            goto next;

        gchar *protocol = NULL;
        gchar *username = NULL;
        gchar *password = NULL;

        if ( libeventd_config_key_file_get_string(config_file, section, "Protocol", &protocol) < 0 )
            goto next;
        if ( libeventd_config_key_file_get_string(config_file, section, "Username", &username) < 0 )
            goto next;
        if ( libeventd_config_key_file_get_string(config_file, section, "Password", &password) < 0 )
            goto next;

        EventdImAccount *account;
        account = g_new0(EventdImAccount, 1);
        account->context = context;

        account->account = purple_account_new(username, protocol);
        if ( password != NULL )
            purple_account_set_password(account->account, password);
        purple_accounts_add(account->account);
        purple_account_set_alias(account->account, *name);
        purple_account_set_enabled(account->account, PACKAGE_NAME, TRUE);

        account->convs = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) purple_conversation_destroy);

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

static void
_eventd_im_event_parse(EventdPluginContext *context, const gchar *config_id, GKeyFile *config_file)
{
    if ( g_key_file_has_group(config_file, "IM") )
    {
        gboolean disable;
        if ( libeventd_config_key_file_get_boolean(config_file, "IM", "Disable", &disable) < 0 )
            return;

        if ( disable )
        {
            g_hash_table_insert(context->events, g_strdup(config_id), NULL);
            return;
        }
    }

    gboolean have_account = FALSE;
    GList *event_accounts = NULL;

    GHashTableIter iter;
    gchar *name, *section;
    EventdImAccount *account;
    g_hash_table_iter_init(&iter, context->accounts);
    while ( g_hash_table_iter_next(&iter, (gpointer *)&name, (gpointer *)&account) )
    {
        section = g_strconcat("IMAccount ", name, NULL);
        if ( ! g_key_file_has_group(config_file, section) )
            goto next;

        have_account = TRUE;

        gchar *message;
        if ( libeventd_config_key_file_get_locale_string(config_file, section, "Message", NULL, &message) != 0 )
            goto next;

        gchar **channels;
        libeventd_config_key_file_get_string_list(config_file, section, "Channels", &channels, NULL);

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
        event_account->message = message;

        if ( channels != NULL )
        {
            gchar **channel;
            for ( channel = channels ; *channel != NULL ; ++channel )
            {
                PurpleConversation *conv;
                conv = g_hash_table_lookup(account->convs, *channel);
                if ( conv == NULL )
                {
                    conv = purple_conversation_new(PURPLE_CONV_TYPE_CHAT, account->account, *channel);

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
    g_hash_table_remove_all(context->accounts);
}

static void
_eventd_im_start(EventdPluginContext *context)
{
    GHashTableIter iter;
    EventdImAccount *account;
    g_hash_table_iter_init(&iter, context->accounts);
    while ( g_hash_table_iter_next(&iter, NULL, (gpointer *)&account) )
        purple_account_connect(account->account);
}

static void
_eventd_im_stop(EventdPluginContext *context)
{
    GHashTableIter iter;
    EventdImAccount *account;
    g_hash_table_iter_init(&iter, context->accounts);
    while ( g_hash_table_iter_next(&iter, NULL, (gpointer *)&account) )
        purple_account_disconnect(account->account);
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

    GList *account_;
    EventdImEventAccount *account;
    for ( account_ = accounts ; account_ != NULL ; account_ = g_list_next(account_) )
    {
        account = account_->data;

        gchar *message;
        message = libeventd_regex_replace_event_data(account->message, event, NULL, NULL);

        GList *conv_;
        for ( conv_ = account->convs ; conv_ != NULL ; conv_ = g_list_next(conv_) )
        {
            PurpleConversation *conv = conv_->data;

            switch ( purple_conversation_get_type(conv) )
            {
            case PURPLE_CONV_TYPE_CHAT:
                purple_conv_chat_send(PURPLE_CONV_CHAT(conv), message);
            break;
            default:
            break;
            }
        }

        g_free(message);
    }
}


/*
 * Plugin interface
 */

EVENTD_EXPORT const gchar *eventd_plugin_id = "eventd-im";
EVENTD_EXPORT
void
eventd_plugin_get_interface(EventdPluginInterface *interface)
{
    libeventd_plugin_interface_add_init_callback(interface, _eventd_im_init);
    libeventd_plugin_interface_add_uninit_callback(interface, _eventd_im_uninit);

    libeventd_plugin_interface_add_global_parse_callback(interface, _eventd_im_global_parse);
    libeventd_plugin_interface_add_event_parse_callback(interface, _eventd_im_event_parse);
    libeventd_plugin_interface_add_config_reset_callback(interface, _eventd_im_config_reset);

    libeventd_plugin_interface_add_start_callback(interface, _eventd_im_start);
    libeventd_plugin_interface_add_stop_callback(interface, _eventd_im_stop);

    libeventd_plugin_interface_add_event_action_callback(interface, _eventd_im_event_action);
}
