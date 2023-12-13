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

#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#ifdef G_OS_UNIX
#include <glib-unix.h>
#else /* ! G_OS_UNIX */
#define UNICODE
#include <windows.h>
#include <process.h>
#include <io.h>
#endif /* ! G_OS_UNIX */
#include <gio/gio.h>

#ifdef ENABLE_SYSTEMD
#include <systemd/sd-daemon.h>
#endif /* ENABLE_SYSTEMD */

#include "nkutils-uuid.h"
#include "nkutils-git-version.h"

#include "eventd-plugin-private.h"
#include "libeventd-helpers-dirs.h"

#include "types.h"

#include "plugins.h"
#include "config_.h"
#include "actions.h"
#include "control.h"
#include "sockets.h"

#include "eventd.h"

#define EVENTD_APP_UUID NK_UUID_MAKE(2c,75,b3,c8,12,59,43,23,b5,de,30,cd,73,c9,48,aa)

#ifdef G_OS_UNIX
#define GIO_UNIX_OPTION_FLAG 0
#else /* ! G_OS_UNIX */
#define GIO_UNIX_OPTION_FLAG G_OPTION_FLAG_HIDDEN
#endif /* ! G_OS_UNIX */

struct _EventdCoreContext {
    EventdCoreInterface iface;
    NkUuid uuid;
    EventdConfig *config;
    EventdControl *control;
    EventdSockets *sockets;
    gboolean system_mode;
    GMainLoop *loop;
    gsize flags_count;
    GQuark *flags;
    EventdControlDelayedStop *delayed_stop;
};

GList *
eventd_core_get_binds(EventdCoreContext *context, const gchar *namespace, const gchar * const *binds)
{
    return eventd_sockets_get_binds(context->sockets, namespace, binds);
}

GList *
eventd_core_get_sockets(EventdCoreContext *context, const gchar *namespace, GSocketAddress **binds)
{
    return eventd_sockets_get_sockets(context->sockets, namespace, binds);
}

const gchar *
eventd_core_get_uuid(EventdCoreContext *context)
{
    return context->uuid.string;
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
    eventd_actions_trigger(context, actions, event);

    return TRUE;
}

gsize
_eventd_core_flags_find(EventdCoreContext *context, GQuark flag)
{
    gsize i;
    for ( i = 0 ; i < context->flags_count ; ++i )
    {
        if ( context->flags[i] == flag )
            break;
    }
    return i;
}

void
eventd_core_flags_add(EventdCoreContext *context, GQuark flag)
{
    if ( eventd_core_flags_test(context, flag) )
        return;

    context->flags = g_renew(GQuark, context->flags, ++context->flags_count + 1);
    context->flags[context->flags_count-1] = flag;
    context->flags[context->flags_count] = 0;
}

void
eventd_core_flags_remove(EventdCoreContext *context, GQuark flag)
{
    gsize i = _eventd_core_flags_find(context, flag);
    if ( i == context->flags_count )
        return;

    for ( ; i < context->flags_count ; ++i )
        context->flags[i] = context->flags[i+1];

    context->flags = g_renew(GQuark, context->flags, --context->flags_count + 1);
}

gboolean
eventd_core_flags_test(EventdCoreContext *context, GQuark flag)
{
    return _eventd_core_flags_find(context, flag) < context->flags_count;
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
    eventd_plugins_start_all();
}

static void
_eventd_core_stop(EventdCoreContext *context)
{
    eventd_plugins_stop_all();

    if ( context->loop != NULL )
        g_main_loop_quit(context->loop);
}

void
eventd_core_stop(EventdCoreContext *context, EventdControlDelayedStop *delayed_stop)
{
    context->delayed_stop = delayed_stop;
    _eventd_core_stop(context);
}

gchar *
eventd_core_dump_event(EventdCoreContext *context, const gchar *event_id)
{
    return eventd_config_dump_event(context->config, event_id);
}

gchar *
eventd_core_dump_action(EventdCoreContext *context, const gchar *action_id)
{
    return eventd_config_dump_action(context->config, action_id);
}


#ifdef EVENTD_DEBUG_OUTPUT
#define PID_MAXLEN 128 + 1 /* \0 */
static void
_eventd_core_debug_log_write(GDataOutputStream *stream, GLogLevelFlags log_level, const gchar *log_domain, const gchar *message)
{
    const gchar *prg_name;
    gchar *line;
    gsize l, o = 0;

    prg_name = g_get_prgname();

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

    l = PID_MAXLEN + strlen("CRITICAL") + strlen(message) + strlen("(:) [] : \n") + 1;
    if ( prg_name != NULL )
        l += strlen(prg_name);
    if ( log_domain != NULL )
        l += strlen(log_domain);

    line = g_newa(gchar, l);

    o += g_snprintf(line + o, l - o, "(");
    if ( prg_name != NULL )
        o += g_snprintf(line + o, l - o, "%s:", prg_name);
    o += g_snprintf(line + o, l - o, "%lu) ", (gulong) getpid());
    if ( log_domain != NULL )
        o += g_snprintf(line + o, l - o, "[%s] ", log_domain);
    o += g_snprintf(line + o, l - o, "%s: %s\n", log_level_message, message);

    g_return_if_fail(o < l);

    g_data_output_stream_put_string(stream, line, NULL, NULL);
}

static void
_eventd_core_debug_log_handler(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data)
{
    g_log_default_handler(log_domain, log_level, message, NULL);
    _eventd_core_debug_log_write(user_data, log_level, log_domain, message);
}

static GLogWriterOutput
_eventd_core_debug_log_writer(GLogLevelFlags log_level, const GLogField *fields, gsize n_fields, gpointer user_data)
{
    g_log_writer_default(log_level, fields, n_fields, NULL);

    const gchar *log_domain = NULL;
    const gchar *message = NULL;
    gsize i;
    for ( i = 0 ; i < n_fields ; ++i )
    {
        if ( g_strcmp0(fields[i].key, "MESSAGE") == 0 )
            message = fields[i].value;
        else if ( g_strcmp0(fields[i].key, "GLIB_DOMAIN") == 0 )
            log_domain = fields[i].value;
    }
    g_return_val_if_fail(message != NULL, G_LOG_WRITER_UNHANDLED);

    _eventd_core_debug_log_write(user_data, log_level, log_domain, message);
    return G_LOG_WRITER_HANDLED;
}
#endif /* ! EVENTD_DEBUG */

#ifdef G_OS_UNIX
static gboolean
_eventd_core_signal_stop(gpointer user_data)
{
    _eventd_core_stop(user_data);
    return G_SOURCE_REMOVE;
}
#endif /* G_OS_UNIX */

int
main(int argc, char *argv[])
{
    EventdCoreContext *context;

    gchar *runtime_dir = NULL;
    gchar *control_socket = NULL;
    gchar **binds = NULL;
    gboolean take_over_socket = FALSE;
    gboolean enable_relay = TRUE;
    gboolean enable_sd_modules = TRUE;
    gchar *config_dir = NULL;
    gboolean systemd_mode = FALSE;
    gboolean daemonize = FALSE;
    gboolean print_paths = FALSE;
    gboolean print_version = FALSE;

    EventdReturnCode retval = EVENTD_RETURN_CODE_OK;
    GError *error = NULL;
    GOptionContext *option_context = NULL;

#ifdef EVENTD_DEBUG_OUTPUT
    g_setenv("G_MESSAGES_DEBUG", "all", FALSE);
#endif /* EVENTD_DEBUG_OUTPUT */

    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, EVENTD_LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");

    if ( ! g_get_filename_charsets(NULL) )
    {
        g_warning(PACKAGE_NAME " does not support non-UTF-8 filename encoding");
        return EVENTD_RETURN_CODE_FILESYSTEM_ENCODING_ERROR;
    }

#ifdef EVENTD_DEBUG_OUTPUT
    setvbuf(stdout, NULL, _IOLBF, 0);
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
            g_log_set_writer_func(_eventd_core_debug_log_writer, debug_stream, NULL);
        }
        g_object_unref(debug_log);
    }
#endif /* EVENTD_DEBUG_OUTPUT */

    context = g_new0(EventdCoreContext, 1);
    context->iface.get_binds = eventd_core_get_binds;
    context->iface.get_sockets = eventd_core_get_sockets;
    context->iface.push_event = eventd_core_push_event;

    NkUuid eventd_uuid = EVENTD_APP_UUID;
    nk_uuid_get_machine_app_specific(&context->uuid, eventd_uuid);

    GOptionEntry entries[] =
    {
        { "private-socket",       'i', 0,                     G_OPTION_ARG_FILENAME,     &control_socket,       "Socket to listen for internal control", "<socket>" },
        { "listen",               'l', 0,                     G_OPTION_ARG_STRING_ARRAY, &binds,                "Add a socket to listen to",             "<socket>" },
        { "take-over",            't', GIO_UNIX_OPTION_FLAG,  G_OPTION_ARG_NONE,         &take_over_socket,     "Take over socket",                      NULL },
        { "no-relay",             0,   G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE,         &enable_relay,         "Disable the relay feature",             NULL },
        { "no-service-discovery", 0,   G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE,         &enable_sd_modules,    "Disable the service discovery feature", NULL },
        { "config-dir",           'c', 0,                     G_OPTION_ARG_FILENAME,     &config_dir,           "Additionnal configuration directory",   "<directory>" },
#ifdef G_OS_UNIX
        { "system",               0,   0,                     G_OPTION_ARG_NONE,         &context->system_mode, NULL,                                    NULL },
#endif /* G_OS_UNIX */
#ifdef ENABLE_SYSTEMD
        { "systemd",              0,   0,                     G_OPTION_ARG_NONE,         &systemd_mode,         NULL,                                    NULL },
#endif /* ENABLE_SYSTEMD */
        { "daemonize",            0,   G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_NONE,         &daemonize,            NULL,                                    NULL },
        { "paths",                'P', 0,                     G_OPTION_ARG_NONE,         &print_paths,          "Print search paths",                    NULL },
        { "version",              'V', 0,                     G_OPTION_ARG_NONE,         &print_version,        "Print version",                         NULL },
        { .long_name = NULL }
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

        dirs = evhelpers_dirs_get_lib("PLUGINS", "plugins");
        g_print("Plugins search paths:");
        for ( dir = dirs ; *dir != NULL ; ++dir )
            g_print("\n    %s", *dir);
        g_print("\n");
        g_strfreev(dirs);

        dirs = evhelpers_dirs_get_config("CONFIG", context->system_mode, NULL);
        g_print("Configuration and events search paths:");
        for ( dir = dirs ; *dir != NULL ; ++dir )
            g_print("\n    %s", *dir);
        if ( ( config_dir != NULL ) && g_file_test(config_dir, G_FILE_TEST_IS_DIR) )
            g_print("\n    %s", config_dir);
        g_print("\n");
        g_strfreev(dirs);

        goto end;
    }

    if ( print_version )
    {
        g_printf(PACKAGE_NAME " " NK_PACKAGE_VERSION "\n");
        goto end;
    }

    if ( context->system_mode )
        g_setenv("XDG_RUNTIME_DIR", "/run", TRUE);
    runtime_dir = g_build_filename(g_get_user_runtime_dir(), PACKAGE_NAME, NULL);
    if ( ( ! g_file_test(runtime_dir, G_FILE_TEST_IS_DIR) ) && ( g_mkdir_with_parents(runtime_dir, context->system_mode ? 0750 : 0700) < 0 ) )
    {
        g_warning("Couldn't create the run dir '%s': %s", runtime_dir, g_strerror(errno));
        retval = EVENTD_RETURN_CODE_NO_RUNTIME_DIR_ERROR;
        goto end;
    }

    context->sockets = eventd_sockets_new(runtime_dir, take_over_socket, systemd_mode);

    context->control = eventd_control_new(context, control_socket);
    if ( context->control == NULL )
    {
        retval = EVENTD_RETURN_CODE_CONTROL_INTERFACE_ERROR;
#ifdef ENABLE_SYSTEMD
        sd_notify(1,
            "STATUS=Failed to start the control interface\n"
        );
#endif /* ENABLE_SYSTEMD */
        goto end;
    }

    eventd_plugins_load(context, (const gchar * const *) binds, enable_relay, enable_sd_modules, context->system_mode);

    context->config = eventd_config_new(config_dir, context->system_mode);

    eventd_plugins_start_all();

#ifdef G_OS_UNIX
    g_unix_signal_add(SIGTERM, _eventd_core_signal_stop, context);
    g_unix_signal_add(SIGINT, _eventd_core_signal_stop, context);

    /* Ignore SIGPIPE as it is useless */
    signal(SIGPIPE, SIG_IGN);
#endif /* G_OS_UNIX */

    if ( daemonize )
    {
        g_print("READY=1\n");
        g_setenv("G_MESSAGES_DEBUG", "", TRUE);
        g_close(0, NULL);
        g_close(1, NULL);
        g_close(2, NULL);
#ifdef G_OS_UNIX
        g_open("/dev/null", O_RDWR, 0);
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

    eventd_config_free(context->config);

    eventd_plugins_unload();

    if ( context->delayed_stop != NULL )
        eventd_control_stop(context->control, context->delayed_stop);

    eventd_control_free(context->control);

end:
    eventd_sockets_free(context->sockets);

    g_free(config_dir);
    g_strfreev(binds);
    g_free(control_socket);
    g_free(runtime_dir);

    g_free(context);

#ifdef EVENTD_DEBUG_OUTPUT
    if ( debug_stream != NULL )
        g_object_unref(debug_stream);
#endif /* EVENTD_DEBUG_OUTPUT */


    return retval;
}
