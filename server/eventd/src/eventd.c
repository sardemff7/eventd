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

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif /* HAVE_LOCALE_H */

#include <glib.h>
#ifdef ENABLE_NLS
#include <glib/gi18n.h>
#endif /* ENABLE_NLS */
#include <glib-object.h>
#include <glib/gprintf.h>
#ifdef G_OS_UNIX
#include <glib-unix.h>
#else /* ! G_OS_UNIX */
#include <process.h>
#include <io.h>
void FreeConsole(void);
#endif /* ! G_OS_UNIX */
#include <gio/gio.h>

#ifdef ENABLE_SYSTEMD
#include <systemd/sd-daemon.h>
#endif /* ENABLE_SYSTEMD */

#include <eventd-plugin-private.h>
#include <eventdctl.h>
#include <libeventd-helpers-dirs.h>

#include "types.h"

#include "plugins.h"
#include "config.h"
#include "actions.h"
#include "control.h"
#include "sockets.h"

#include "eventd.h"

#ifdef G_OS_UNIX
#define GIO_UNIX_OPTION_FLAG 0
#else /* ! G_OS_UNIX */
#define GIO_UNIX_OPTION_FLAG G_OPTION_FLAG_HIDDEN
#endif /* ! G_OS_UNIX */

struct _EventdCoreContext {
    EventdCoreInterface interface;
    EventdConfig *config;
    EventdControl *control;
    EventdSockets *sockets;
    gchar **binds;
    gboolean system_mode;
    GMainLoop *loop;
    guint flags_count;
    GQuark *flags;
};

GList *
eventd_core_get_binds(EventdCoreContext *context, const gchar * const *binds)
{
    return eventd_sockets_get_binds(context->sockets, binds);
}

GList *
eventd_core_get_sockets(EventdCoreContext *context, GSocketAddress **binds)
{
    return eventd_sockets_get_sockets(context->sockets, binds);
}

gboolean
eventd_core_push_event(EventdCoreContext *context, EventdEvent *event)
{
    const gchar *category;
    category = eventd_event_get_category(event);
    if ( category[0] == '.' )
    {
        eventd_plugins_event_dispatch_all(event);
        return TRUE;
    }

    const GList *actions;
    if ( ! eventd_config_process_event(context->config, event, context->flags, &actions) )
        return FALSE;

    eventd_plugins_event_dispatch_all(event);
    eventd_actions_trigger(actions, event);

    return TRUE;
}

void
eventd_core_flags_add(EventdCoreContext *context, GQuark flag)
{
    context->flags = g_renew(GQuark, context->flags, ++context->flags_count + 1);
    context->flags[context->flags_count-1] = flag;
    context->flags[context->flags_count] = 0;
}

void
eventd_core_flags_remove(EventdCoreContext *context, GQuark flag)
{
    guint i;
    for ( i = 0 ; i < context->flags_count ; ++i )
    {
        if ( context->flags[i] == flag )
            break;
    }

    for ( ; i < context->flags_count ; ++i )
        context->flags[i] = context->flags[i+1];

    context->flags = g_renew(GQuark, context->flags, --context->flags_count + 1);
}

void
eventd_core_flags_reset(EventdCoreContext *context)
{
    g_free(context->flags);
    context->flags = NULL;
    context->flags_count = 0;
}

gchar *
eventd_core_flags_list(EventdCoreContext *context)
{
    GString *r;

    r = g_string_new("Flags list: ");
    guint i;
    if ( context->flags_count == 0 )
        g_string_append(r, "(empty)");
    else
    {
        for ( i = 0 ; i < context->flags_count ; ++i )
            g_string_append(g_string_append(r, g_quark_to_string(context->flags[i])), ", ");
        g_string_truncate(r, r->len - 2);
    }

    return g_string_free(r, FALSE);
}

void
eventd_core_config_reload(EventdCoreContext *context)
{
    eventd_plugins_stop_all();
    eventd_config_parse(context->config, context->system_mode);
    eventd_plugins_start_all((const gchar * const *) context->binds);
}

void
eventd_core_stop(EventdCoreContext *context)
{
    eventd_plugins_stop_all();

    eventd_control_stop(context->control);

    if ( context->loop != NULL )
        g_main_loop_quit(context->loop);
}

#ifdef EVENTD_DEBUG
#define PID_MAXLEN 128 + 1 /* \0 */
static void
_eventd_core_debug_log_handler(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data)
{
    GDataOutputStream *stream = user_data;

    g_log_default_handler(log_domain, log_level, message, NULL);

    const gchar *prg_name;
    gchar pid[PID_MAXLEN];
    prg_name = g_get_prgname();
    g_snprintf(pid, PID_MAXLEN, "%lu", (gulong) getpid());

    g_data_output_stream_put_string(stream, "(", NULL, NULL);
    if ( prg_name != NULL )
    {
        g_data_output_stream_put_string(stream, prg_name, NULL, NULL);
        g_data_output_stream_put_string(stream, ":", NULL, NULL);
    }
    else
        g_data_output_stream_put_string(stream, "process:", NULL, NULL);
    g_data_output_stream_put_string(stream, pid, NULL, NULL);
    g_data_output_stream_put_string(stream, ") ", NULL, NULL);

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
    g_data_output_stream_put_string(stream, log_level_message, NULL, NULL);

    if ( log_domain != NULL )
    {
        g_data_output_stream_put_string(stream, " [", NULL, NULL);
        g_data_output_stream_put_string(stream, log_domain, NULL, NULL);
        g_data_output_stream_put_string(stream, "]", NULL, NULL);
    }

    g_data_output_stream_put_string(stream, ": ", NULL, NULL);
    g_data_output_stream_put_string(stream, message, NULL, NULL);
    g_data_output_stream_put_byte(stream, '\n', NULL, NULL);
}

#endif /* ! EVENTD_DEBUG */

#ifdef G_OS_UNIX
static gboolean
_eventd_core_stop(gpointer user_data)
{
    eventd_core_stop(user_data);
    return FALSE;
}
#endif /* G_OS_UNIX */

int
main(int argc, char *argv[])
{
    EventdCoreContext *context;

    gchar *runtime_dir = NULL;
    gchar *control_socket = NULL;
    gboolean take_over_socket = FALSE;
    gboolean enable_relay = TRUE;
    gboolean enable_sd_modules = TRUE;
    gboolean daemonize = FALSE;
    gboolean print_paths = FALSE;
    gboolean print_version = FALSE;

    EventdReturnCode retval = EVENTD_RETURN_CODE_OK;
    GError *error = NULL;
    GOptionContext *option_context = NULL;

#ifdef EVENTD_DEBUG
    g_setenv("G_MESSAGES_DEBUG", "all", FALSE);
#endif /* EVENTD_DEBUG */

    setlocale(LC_ALL, "");
#ifdef ENABLE_NLS
    bindtextdomain(GETTEXT_PACKAGE, EVENTD_LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
#endif /* ENABLE_NLS */

    if ( ! g_get_filename_charsets(NULL) )
    {
        g_warning(PACKAGE_NAME " does not support non-UTF-8 filename encoding");
        return EVENTD_RETURN_CODE_FILESYSTEM_ENCODING_ERROR;
    }

    if ( G_UNLIKELY(! g_module_supported()) )
    {
        g_warning("No loadable module support");
        return EVENTD_RETURN_CODE_NO_MODULE_SUPPORT_ERROR;
    }

#ifdef EVENTD_DEBUG
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
#endif /* EVENTD_DEBUG */

    context = g_new0(EventdCoreContext, 1);
    context->interface.get_sockets = eventd_core_get_sockets;
    context->interface.push_event = eventd_core_push_event;

#ifdef G_OS_UNIX
    context->system_mode = ( g_getenv("XDG_RUNTIME_DIR") == NULL );
    if ( context->system_mode )
        g_setenv("XDG_RUNTIME_DIR", "/run", TRUE);
#endif /* G_OS_UNIX */

    GOptionEntry entries[] =
    {
        { "private-socket",       'i', 0,                     G_OPTION_ARG_FILENAME,     &control_socket,    "Socket to listen for internal control", "<socket>" },
        { "listen",               'l', 0,                     G_OPTION_ARG_STRING_ARRAY, &context->binds,    "Add a socket to listen to",             "<socket>" },
        { "take-over",            't', GIO_UNIX_OPTION_FLAG,  G_OPTION_ARG_NONE,         &take_over_socket,  "Take over socket",                      NULL },
        { "no-relay",             0,   G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE,         &enable_relay,      "Disable the relay feature",             NULL },
        { "no-service-discovery", 0,   G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE,         &enable_sd_modules, "Disable the service discovery feature", NULL },
        { "daemonize",            0,   G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_NONE,         &daemonize,         NULL,                                    NULL },
        { "paths",                'P', 0,                     G_OPTION_ARG_NONE,         &print_paths,       "Print search paths",                    NULL },
        { "version",              'V', 0,                     G_OPTION_ARG_NONE,         &print_version,     "Print version",                         NULL },
        { NULL }
    };

    option_context = g_option_context_new("- small daemon to act on remote or local events");
    g_option_context_add_main_entries(option_context, entries, GETTEXT_PACKAGE);
    if ( ! g_option_context_parse(option_context, &argc, &argv, &error) )
    {
        g_warning("Option parsing failed: %s\n", error->message);
        retval = EVENTD_RETURN_CODE_ARGV_ERROR;
        goto end;
    }
    g_option_context_free(option_context);

    if ( print_paths )
    {
        gchar **dirs, **dir;

        dirs = evhelpers_dirs_get_lib("EVENTD_PLUGINS_DIR", "plugins");
        g_print("Plugins search paths:");
        for ( dir = dirs ; *dir != NULL ; ++dir )
            g_print("\n    %s", *dir);
        g_print("\n");
        g_strfreev(dirs);

        dirs = evhelpers_dirs_get_config("EVENTD_CONFIG_DIR", context->system_mode, NULL);
        g_print("Configuration and events search paths:");
        for ( dir = dirs ; *dir != NULL ; ++dir )
            g_print("\n    %s", *dir);
        g_print("\n");
        g_strfreev(dirs);

        goto end;
    }

    if ( print_version )
    {
        g_printf(PACKAGE_NAME " " EVENTD_VERSION "\n");
        goto end;
    }

    runtime_dir = g_build_filename(g_get_user_runtime_dir(), PACKAGE_NAME, NULL);
    if ( ( ! g_file_test(runtime_dir, G_FILE_TEST_IS_DIR) ) && ( g_mkdir_with_parents(runtime_dir, 0755) < 0 ) )
    {
        g_warning("Couldn't create the run dir '%s': %s", runtime_dir, g_strerror(errno));
        retval = EVENTD_RETURN_CODE_NO_RUNTIME_DIR_ERROR;
        goto end;
    }

    eventd_plugins_load(context, enable_relay, enable_sd_modules, context->system_mode);

    context->config = eventd_config_new(context->system_mode);

    context->sockets = eventd_sockets_new(runtime_dir, take_over_socket);

    context->control = eventd_control_new(context);

    if ( eventd_control_start(context->control, control_socket) )
    {
        eventd_plugins_start_all((const gchar * const *) context->binds);

#ifdef G_OS_UNIX
        g_unix_signal_add(SIGTERM, _eventd_core_stop, context);
        g_unix_signal_add(SIGINT, _eventd_core_stop, context);

        /* Ignore SIGPIPE as it is useless */
        signal(SIGPIPE, SIG_IGN);
#endif /* G_OS_UNIX */

        if ( daemonize )
        {
            g_print("READY=1\n");
            g_setenv("G_MESSAGES_DEBUG", "", TRUE);
            close(0);
            close(1);
            close(2);
#ifdef G_OS_UNIX
            open("/dev/null", O_RDWR);
            dup2(0,1);
            dup2(0,2);
#else /* ! G_OS_UNIX */
            FreeConsole();
#endif /* ! G_OS_UNIX */
        }

#ifdef ENABLE_SYSTEMD
        sd_notify(1,
            "READY=1\n"
            "STATUS=Waiting for events\n"
        );
#endif /* ENABLE_SYSTEMD */

        context->loop = g_main_loop_new(NULL, FALSE);
        g_main_loop_run(context->loop);
        g_main_loop_unref(context->loop);
    }
    else
    {
        retval = EVENTD_RETURN_CODE_CONTROL_INTERFACE_ERROR;
#ifdef ENABLE_SYSTEMD
        sd_notify(1,
            "STATUS=Failed to start the control interface\n"
        );
#endif /* ENABLE_SYSTEMD */
    }

    eventd_control_free(context->control);

    eventd_sockets_free(context->sockets);

    eventd_config_free(context->config);

    eventd_plugins_unload();

end:
    g_strfreev(context->binds);
    g_free(control_socket);
    g_free(runtime_dir);

    g_free(context);

#ifdef EVENTD_DEBUG
    if ( debug_stream != NULL )
        g_object_unref(debug_stream);
#endif /* EVENTD_DEBUG */


    return retval;
}
