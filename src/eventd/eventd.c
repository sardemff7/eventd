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

#include <glib.h>
#include <glib-object.h>
#include <glib/gprintf.h>
#ifdef G_OS_UNIX
#include <glib-unix.h>
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

struct _EventdCoreContext {
    EventdConfig *config;
    EventdControl *control;
    EventdQueue *queue;
    EventdSockets *sockets;
    gchar *runtime_dir;
    gboolean take_over_socket;
    GMainLoop *loop;
};

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

            gint64 parsed_value;
            guint16 port;

            parsed_value = g_ascii_strtoll(bind+4, NULL, 10);
            port = CLAMP(parsed_value, 0, 65535);
            if ( port > 0 )
            {
                GList *inet_sockets;

                inet_sockets = eventd_sockets_get_inet_sockets(context->sockets, NULL, port);

                if ( inet_sockets != NULL )
                    sockets = g_list_concat(sockets, inet_sockets);
            }
        }
#if ENABLE_GIO_UNIX
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
#endif /* ENABLE_GIO_UNIX */
    }

    return sockets;
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

#ifdef G_OS_UNIX
static gboolean
_eventd_core_quit(gpointer user_data)
{
    eventd_core_quit(user_data);
    return FALSE;
}
#endif /* G_OS_UNIX */

int
main(int argc, char *argv[])
{
    EventdCoreContext *context;
    EventdCoreInterface interface =
    {
        .get_sockets = eventd_core_get_sockets,

        .push_event = eventd_core_push_event,
    };


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

    context = g_new0(EventdCoreContext, 1);

    GOptionEntry entries[] =
    {
#if ENABLE_GIO_UNIX
        { "take-over", 't', 0, G_OPTION_ARG_NONE, &context->take_over_socket, "Take over socket", NULL },
#endif /* ENABLE_GIO_UNIX */
        { "version", 'V', 0, G_OPTION_ARG_NONE, &print_version, "Print version", NULL },
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
        g_warning("Couldn’t create the run dir '%s': %s", context->runtime_dir, strerror(errno));
        g_free(context->runtime_dir);
        context->runtime_dir = NULL;
    }

#ifdef G_OS_UNIX
    g_unix_signal_add(SIGTERM, _eventd_core_quit, context);
    g_unix_signal_add(SIGINT, _eventd_core_quit, context);
#endif /* G_OS_UNIX */

    eventd_config_parse(context->config);

    context->sockets = eventd_sockets_new();

    eventd_control_start(context->control);

    eventd_queue_start(context->queue);

    eventd_plugins_start_all();

    context->loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(context->loop);
    g_main_loop_unref(context->loop);

    eventd_sockets_free(context->sockets);

    g_free(context->runtime_dir);
end:
    eventd_queue_free(context->queue);
    eventd_config_free(context->config);

    eventd_plugins_unload();

    eventd_control_free(context->control);

    g_free(context);

    return retval;
}
