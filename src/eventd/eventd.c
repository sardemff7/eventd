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

#include <glib.h>
#include <glib-object.h>
#include <glib/gprintf.h>
#ifdef G_OS_UNIX
#include <glib-unix.h>
#endif /* G_OS_UNIX */

#include "types.h"

#include "plugins.h"
#include "config.h"
#include "control.h"
#include "queue.h"
#include "sockets.h"
#include "service.h"
#include "dbus.h"

#include "eventd.h"

struct _EventdCoreContext {
    EventdConfig *config;
    EventdControl *control;
    EventdQueue *queue;
    EventdService *service;
    EventdDbusContext *dbus;
    GMainLoop *loop;
};

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

    eventd_service_stop(context->service);

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

    guint16 bind_port = DEFAULT_BIND_PORT;

    gchar *private_socket = NULL;
    gchar *unix_socket = NULL;
    gboolean take_over_socket = FALSE;

    gboolean no_avahi = FALSE;

    gboolean print_version = FALSE;

    GOptionEntry entries[] =
    {
        { "port", 'p', 0, G_OPTION_ARG_INT, &bind_port, "Port to listen for inbound connections", "P" },
#if ENABLE_GIO_UNIX
        { "private-socket", 'i', 0, G_OPTION_ARG_FILENAME, &private_socket, "UNIX socket to listen for internal control", "SOCKET_FILE" },
        { "socket", 's', 0, G_OPTION_ARG_FILENAME, &unix_socket, "UNIX socket to listen for inbound connections", "SOCKET_FILE" },
        { "take-over", 't', 0, G_OPTION_ARG_NONE, &take_over_socket, "Take over socket", NULL },
#endif /* ENABLE_GIO_UNIX */
#if ENABLE_AVAHI
        { "no-avahi", 'A', 0, G_OPTION_ARG_NONE, &no_avahi, "Disable avahi publishing", NULL },
#endif /* ENABLE_AVAHI */
        { "version", 'V', 0, G_OPTION_ARG_NONE, &print_version, "Print version", NULL },
        { NULL }
    };

    int retval = 0;
    GError *error = NULL;
    GOptionContext *option_context = NULL;
    GOptionGroup *option_group;
    GList *sockets = NULL;

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

    context->control = eventd_control_new(context);

    if ( g_getenv("EVENTD_NO_PLUGINS") == NULL )
        eventd_plugins_load();

    context->config = eventd_config_new();
    context->queue = eventd_queue_new(context->config);

    context->service = eventd_service_new(context, context->config);
    context->dbus = eventd_dbus_new(context, context->config);


    option_context = g_option_context_new("- small daemon to act on remote or local events");

    option_group = g_option_group_new(NULL, NULL, NULL, NULL, NULL);
    g_option_group_set_translation_domain(option_group, GETTEXT_PACKAGE);
    g_option_group_add_entries(option_group, entries);
    g_option_context_set_main_group(option_context, option_group);

    if ( ! g_option_context_parse(option_context, &argc, &argv, &error) )
        g_error("Option parsing failed: %s\n", error->message);
    g_option_context_free(option_context);

    if ( print_version )
    {
        g_fprintf(stdout, PACKAGE_NAME " " PACKAGE_VERSION "\n");
        goto end;
    }


#ifdef G_OS_UNIX
    g_unix_signal_add(SIGTERM, _eventd_core_quit, context);
    g_unix_signal_add(SIGINT, _eventd_core_quit, context);
#endif /* G_OS_UNIX */

    eventd_config_parse(context->config);

    sockets = eventd_sockets_get_all(bind_port, &private_socket, &unix_socket, take_over_socket);

    eventd_control_start(context->control, &sockets);

    eventd_queue_start(context->queue);

    eventd_plugins_start_all();

    eventd_service_start(context->service, sockets, no_avahi);
    eventd_dbus_start(context->dbus);

    context->loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(context->loop);
    g_main_loop_unref(context->loop);

    eventd_dbus_stop(context->dbus);

    eventd_sockets_free_all(sockets, unix_socket, private_socket);

end:
    eventd_dbus_free(context->dbus);
    eventd_service_free(context->service);

    eventd_queue_free(context->queue);
    eventd_config_free(context->config);

    eventd_plugins_unload();

    eventd_control_free(context->control);

    g_free(context);

    return retval;
}
