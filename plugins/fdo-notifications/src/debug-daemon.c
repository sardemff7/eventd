/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2018 Quentin "Sardem FF7" Glidic
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

#include "libeventd-event.h"
#include "libeventc.h"
#include "eventd-plugin.h"
#include "eventd-plugin-private.h"

struct _EventdCoreContext {
    EventdCoreInterface interface;
    GDBusConnection *dbus;
    gchar *owner_name;
    EventdPluginContext *plugin;
    EventdPluginInterface plugin_interface;
    EventcConnection *client;
    gsize connection_count;
    GMainLoop *loop;
};

static gboolean
_eventd_fdo_notifications_debug_daemon_push_event(EventdPluginCoreContext *context, EventdEvent *event)
{
    GError *error = NULL;
    if ( ! eventc_connection_send_event(context->client, event, &error) )
    {
        g_warning("Error forwarding event: %s", error->message);
        g_clear_error(&error);
        return FALSE;
    }

    return TRUE;
}

static gboolean
_eventd_fdo_notifications_debug_daemon_idle_stop(gpointer user_data)
{
    EventdPluginCoreContext *context = user_data;

    g_main_loop_quit(context->loop);

    return G_SOURCE_REMOVE;
}

static void
_eventd_fdo_notifications_debug_daemon_stop(EventdPluginCoreContext *context)
{
    context->plugin_interface.stop(context->plugin);
    g_idle_add(_eventd_fdo_notifications_debug_daemon_idle_stop, context);
}

static gboolean _eventd_fdo_notifications_debug_daemon_try_connect(gpointer user_data);

static void
_eventd_fdo_notifications_debug_daemon_connect(G_GNUC_UNUSED GObject *obj, GAsyncResult *res, gpointer user_data)
{
    EventdPluginCoreContext *context = user_data;
    GError *error = NULL;

    if ( eventc_connection_connect_finish(context->client, res, &error) )
    {
        context->connection_count = 0;

        gchar *argv[] = {
            "eventdctl",
            "notify",
            "set-ignore",
            context->owner_name,
            NULL
        };
        g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);

        return;
    }

    g_warning("Could not connect to eventd: %s", error->message);
    g_clear_error(&error);

    if ( ++context->connection_count > 5 )
    {
        g_warning("Too many attemps, quitting");
        _eventd_fdo_notifications_debug_daemon_stop(context);
        return;
    }

    g_timeout_add_seconds(1 << context->connection_count, _eventd_fdo_notifications_debug_daemon_try_connect, context);
}

static gboolean
_eventd_fdo_notifications_debug_daemon_try_connect(gpointer user_data)
{
    EventdPluginCoreContext *context = user_data;

    eventc_connection_connect(context->client, _eventd_fdo_notifications_debug_daemon_connect, context);

    return G_SOURCE_REMOVE;
}

static void
_eventd_fdo_notifications_debug_daemon_disconnected(EventdPluginCoreContext *context, G_GNUC_UNUSED GObject *obj)
{
    eventc_connection_connect(context->client, _eventd_fdo_notifications_debug_daemon_connect, context);
}

static void
_eventd_fdo_notifications_debug_daemon_event(EventdPluginCoreContext *context, EventdEvent *event, G_GNUC_UNUSED GObject *obj)
{
    context->plugin_interface.event_dispatch(context->plugin, event);
}

int
main(int argc, char *argv[])
{
    EventdPluginCoreContext core = {
        .interface = {
            .push_event = _eventd_fdo_notifications_debug_daemon_push_event,
        },
    };
    GError *error = NULL;

    /* Only built with debug enabled */
    g_setenv("G_MESSAGES_DEBUG", "all", FALSE);

    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, EVENTD_LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");

    core.dbus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if ( core.dbus == NULL )
    {
        g_warning("Could not initiate D-Bus connection: %s", error->message);
        g_clear_error(&error);
        return 1;
    }
    core.owner_name = g_strdup(g_dbus_connection_get_unique_name(core.dbus));

    eventd_plugin_get_interface(&core.plugin_interface);
    core.plugin = core.plugin_interface.init(&core);
    if ( core.plugin == NULL )
    {
        g_warning("Could not initiate plugin");
        return 2;
    }

    core.client = eventc_connection_new(NULL, &error);
    if ( core.client == NULL )
    {
        g_warning("Could not initiate connection: %s", error->message);
        g_clear_error(&error);
        return 3;
    }

    eventc_connection_set_subscribe(core.client, TRUE);

    eventc_connection_connect(core.client, _eventd_fdo_notifications_debug_daemon_connect, &core);

    g_signal_connect_swapped(core.client, "disconnected", G_CALLBACK(_eventd_fdo_notifications_debug_daemon_disconnected), &core);
    g_signal_connect_swapped(core.client, "received-event", G_CALLBACK(_eventd_fdo_notifications_debug_daemon_event), &core);

    core.plugin_interface.start(core.plugin);

    core.loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(core.loop);
    g_main_loop_unref(core.loop);

    eventc_connection_close(core.client, NULL);

    core.plugin_interface.uninit(core.plugin);

    return 0;
}
