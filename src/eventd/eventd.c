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

#include <errno.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <glib.h>
#include <glib-object.h>
#include <glib/gprintf.h>
#ifdef G_OS_UNIX
#if GLIB_CHECK_VERSION(2,32,0)
#include <glib-unix.h>
#endif /* GLIB_CHECK_VERSION(2,32,0) */
#endif /* G_OS_UNIX */
#include <gio/gio.h>

#include <eventd-core-interface.h>

#include "types.h"

#include "plugins.h"
#include "config.h"
#include "control.h"
#include "queue.h"
#include "sockets.h"

#include "eventd.h"

#if HAVE_GIO_UNIX
#define GIO_UNIX_OPTION_FLAG 0
#else /* ! HAVE_GIO_UNIX */
#define GIO_UNIX_OPTION_FLAG G_OPTION_FLAG_HIDDEN
#endif /* ! HAVE_GIO_UNIX */

struct _EventdCoreContext {
    EventdConfig *config;
    EventdControl *control;
    EventdQueue *queue;
    EventdSockets *sockets;
    gchar *runtime_dir;
    gboolean take_over_socket;
    GMainLoop *loop;
};

static gboolean
_eventd_core_get_inet_address(const gchar *bind, gchar **address, guint16 *port)
{
    const gchar *address_port;

    address_port = g_strrstr(bind, ":");
    if ( address_port != NULL )
        ++address_port;
    else
        address_port = bind;

    gint64 parsed_value;

    parsed_value = g_ascii_strtoll(address_port, NULL, 10);
    *port = CLAMP(parsed_value, 0, 65535);

    if ( *port == 0 )
        return FALSE;

    if ( bind[0] == '[' )
    {
        /*
         * This is an IPv6 address
         * we remove the enclosing square bracets
         */
        ++bind;
        --address_port;
    }
    if ( --address_port > bind )
        *address = g_strndup(bind, address_port - bind);
    else
        *address = NULL;

    return TRUE;
}

GList *
eventd_core_get_sockets(EventdCoreContext *context, const gchar * const *binds)
{
    GList *sockets = NULL;
    const gchar * const * bind_;

    if ( binds == NULL )
        return NULL;

    for ( bind_ = binds ; *bind_ != NULL ; ++bind_ )
    {
        const gchar *bind = *bind_;

        if ( *bind == 0 )
            continue;

        if ( g_str_has_prefix(bind, "tcp:") )
        {
            if ( bind[4] == 0 )
                continue;
            gchar *address;
            guint16 port;

            if ( ! _eventd_core_get_inet_address(bind+4, &address, &port) )
                continue;

            GList *inet_sockets;

            inet_sockets = eventd_sockets_get_inet_sockets(context->sockets, address, port);
            g_free(address);

            if ( inet_sockets != NULL )
                sockets = g_list_concat(sockets, inet_sockets);
        }
#if HAVE_GIO_UNIX
        else if ( g_str_has_prefix(bind, "unix:") )
        {
            if ( bind[5] == 0 )
                continue;

            GSocket *socket;

            socket = eventd_sockets_get_unix_socket(context->sockets, bind+5, context->take_over_socket);

            if ( socket != NULL )
                sockets = g_list_prepend(sockets, socket);
        }
        else if ( g_str_has_prefix(bind, "unix-runtime:") )
        {
            if ( bind[13] == 0 )
                continue;

            GSocket *socket;
            gchar *path;

            path = g_build_filename(context->runtime_dir, bind+13, NULL);
            socket = eventd_sockets_get_unix_socket(context->sockets, path, context->take_over_socket);
            g_free(path);

            if ( socket != NULL )
                sockets = g_list_prepend(sockets, socket);
        }
#endif /* HAVE_GIO_UNIX */
    }

    return sockets;
}

const gchar *
eventd_core_get_event_config_id(EventdCoreContext *context, EventdEvent *event)
{
    return eventd_config_get_event_config_id(context->config, event);
}

void
eventd_core_push_event(EventdCoreContext *context, EventdEvent *event)
{
    eventd_queue_push(context->queue, event);
}

void
eventd_core_config_reload(EventdCoreContext *context)
{
    eventd_plugins_stop_all();
    eventd_config_parse(context->config);
    eventd_plugins_start_all();
}

void
eventd_core_quit(EventdCoreContext *context)
{
    eventd_queue_stop(context->queue);

    eventd_plugins_stop_all();

    eventd_control_stop(context->control);

    if ( context->loop != NULL )
        g_main_loop_quit(context->loop);
}

#if DEBUG
static void
_eventd_core_debug_log_handler(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data)
{
    GDataOutputStream *debug_stream = user_data;

    g_log_default_handler(log_domain, log_level, message, NULL);

    const gchar *log_level_message = "";

    switch ( log_level & G_LOG_LEVEL_MASK )
    {
        case G_LOG_LEVEL_ERROR:
            log_level_message = "ERROR";
        break;
        case G_LOG_LEVEL_CRITICAL:
            log_level_message = "CRITICAL";
        break;
        case G_LOG_LEVEL_WARNING:
            log_level_message = "WARNING";
        break;
        case G_LOG_LEVEL_MESSAGE:
            log_level_message = "MESSAGE";
        break;
        case G_LOG_LEVEL_INFO:
            log_level_message = "INFO";
        break;
        case G_LOG_LEVEL_DEBUG:
            log_level_message = "DEBUG";
        break;
    }

    GString *full_message;
    const gchar *prg_name;

    full_message = g_string_new(NULL);
    prg_name = g_get_prgname();

    if ( prg_name != NULL )
        g_string_append_printf(full_message, "(%s:%lu): ", prg_name, (gulong) getpid());
    else
        g_string_append_printf(full_message, "(process:%lu): ", (gulong) getpid());

    g_string_append(full_message, log_level_message);

    if ( log_domain != NULL )
        g_string_append_printf(full_message, " [%s]", log_domain);

    g_string_append(full_message, ": ");
    g_string_append(full_message, message);
    g_string_append(full_message, "\n");

    g_data_output_stream_put_string(debug_stream, full_message->str, NULL, NULL);

    g_string_free(full_message, TRUE);
}

#endif /* ! DEBUG */

#ifdef G_OS_UNIX
#if GLIB_CHECK_VERSION(2,32,0)
static gboolean
_eventd_core_quit(gpointer user_data)
{
    eventd_core_quit(user_data);
    return FALSE;
}
#else /* ! GLIB_CHECK_VERSION(2,32,0) */
static EventdCoreContext *_eventd_core_sigaction_context = NULL;
static void
_eventd_core_sigaction_quit(int sig, siginfo_t *info, void *data)
{
    eventd_core_quit(_eventd_core_sigaction_context);
}
#endif /* ! GLIB_CHECK_VERSION(2,32,0) */
#endif /* G_OS_UNIX */

int
main(int argc, char *argv[])
{
    EventdCoreContext *context;
    EventdCoreInterface interface =
    {
        .get_sockets = eventd_core_get_sockets,

        .get_event_config_id = eventd_core_get_event_config_id,
        .push_event = eventd_core_push_event,
    };


    gboolean daemonize = FALSE;
    gboolean print_version = FALSE;

    int retval = 0;
    GError *error = NULL;
    GOptionContext *option_context = NULL;
    GOptionGroup *option_group;

#if DEBUG
    g_setenv("G_MESSAGES_DEBUG", "all", FALSE);
#endif /* ! DEBUG */

#if ENABLE_NLS
    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
#endif /* ENABLE_NLS */

    g_type_init();

#if DEBUG
    const gchar *debug_log_filename =  g_getenv("EVENTD_DEBUG_LOG_FILENAME");
    GDataOutputStream *debug_stream = NULL;

    if ( debug_log_filename != NULL )
    {
        GFile *debug_log;

        debug_log = g_file_new_for_path(debug_log_filename);

        GError *error = NULL;
        GFileOutputStream *debug_log_stream;

        debug_log_stream = g_file_append_to(debug_log, G_FILE_CREATE_NONE, NULL, &error);

        if ( debug_log_stream == NULL )
        {
            g_warning("Couldn't open debug log file: %s", error->message);
            g_clear_error(&error);
        }
        else
        {
            debug_stream = g_data_output_stream_new(G_OUTPUT_STREAM(debug_log_stream));
            g_object_unref(debug_log_stream);

            g_log_set_default_handler(_eventd_core_debug_log_handler, debug_stream);
        }
        g_object_unref(debug_log);
    }
#endif /* DEBUG */

    context = g_new0(EventdCoreContext, 1);

    GOptionEntry entries[] =
    {
        { "take-over", 't', GIO_UNIX_OPTION_FLAG, G_OPTION_ARG_NONE, &context->take_over_socket, "Take over socket", NULL },
        { "daemonize", 0,   G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &daemonize,                 NULL,               NULL },
        { "version",   'V', 0,                    G_OPTION_ARG_NONE, &print_version,             "Print version",    NULL },
        { NULL }
    };

    context->control = eventd_control_new(context);

    if ( g_getenv("EVENTD_NO_PLUGINS") == NULL )
        eventd_plugins_load(context, &interface);

    context->config = eventd_config_new();
    context->queue = eventd_queue_new(context->config);


    option_context = g_option_context_new("- small daemon to act on remote or local events");

    option_group = g_option_group_new(NULL, NULL, NULL, NULL, NULL);
    g_option_group_set_translation_domain(option_group, GETTEXT_PACKAGE);
    g_option_group_add_entries(option_group, entries);
    eventd_control_add_option_entry(context->control, option_group);
    g_option_context_set_main_group(option_context, option_group);

    eventd_plugins_add_option_group_all(option_context);

    if ( ! g_option_context_parse(option_context, &argc, &argv, &error) )
        g_error("Option parsing failed: %s\n", error->message);
    g_option_context_free(option_context);

    if ( print_version )
    {
        g_fprintf(stdout, PACKAGE_NAME " " PACKAGE_VERSION "\n");
        goto end;
    }


    context->runtime_dir = g_build_filename(g_get_user_runtime_dir(), PACKAGE_NAME, NULL);
    if ( ( ! g_file_test(context->runtime_dir, G_FILE_TEST_IS_DIR) ) && ( g_mkdir_with_parents(context->runtime_dir, 0755) < 0 ) )
    {
        g_warning("Couldn't create the run dir '%s': %s", context->runtime_dir, g_strerror(errno));
        g_free(context->runtime_dir);
        context->runtime_dir = NULL;
    }

#ifdef G_OS_UNIX
#if GLIB_CHECK_VERSION(2,32,0)
    g_unix_signal_add(SIGTERM, _eventd_core_quit, context);
    g_unix_signal_add(SIGINT, _eventd_core_quit, context);
#else /* ! GLIB_CHECK_VERSION(2,32,0) */
    _eventd_core_sigaction_context = context;
    struct sigaction action;
    action.sa_sigaction = _eventd_core_sigaction_quit;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);
#endif /* ! GLIB_CHECK_VERSION(2,32,0) */
#endif /* G_OS_UNIX */

    eventd_config_parse(context->config);

    context->sockets = eventd_sockets_new();

    if ( ! eventd_control_start(context->control) )
        goto no_control;

    eventd_queue_start(context->queue);

    eventd_plugins_start_all();

    if ( daemonize )
    {
        close(0);
        close(1);
        close(2);
    }

    context->loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(context->loop);
    g_main_loop_unref(context->loop);

no_control:
    eventd_sockets_free(context->sockets);

    g_free(context->runtime_dir);
end:
    eventd_queue_free(context->queue);
    eventd_config_free(context->config);

    eventd_plugins_unload();

    eventd_control_free(context->control);

    g_free(context);

#if DEBUG
    if ( debug_stream != NULL )
        g_object_unref(debug_stream);
#endif /* DEBUG */

    return retval;
}
