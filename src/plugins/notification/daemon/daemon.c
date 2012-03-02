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
#include <gmodule.h>
#include <cairo.h>

#include <libeventd-event.h>

#include "../notification.h"

#include "types.h"
#include "bubble.h"
#include "style.h"
#include "backends/backend.h"

#include "daemon.h"

struct _EventdNdContext {
    EventdNdStyle *style;
    GHashTable *bubbles;
    GList *backends;
    GList *displays;
};

void
_eventd_nd_backends_load_dir(EventdNdContext *context, const gchar *backends_dir_name)
{
    GError *error;
    GDir *plugins_dir;
    const gchar *file;


#if DEBUG
    g_debug("Scannig notification backends dir: %s", backends_dir_name);
#endif /* DEBUG */

    plugins_dir = g_dir_open(backends_dir_name, 0, &error);
    if ( ! plugins_dir )
    {
        g_warning("Can’t read the plugins directory: %s", error->message);
        g_clear_error(&error);
        return;
    }

    while ( ( file = g_dir_read_name(plugins_dir) ) != NULL )
    {
        gchar *full_filename;
        EventdNdBackend *backend;
        EventdNdBackendGetInfoFunc backend_get_info;
        GModule *module;

        full_filename = g_build_filename(backends_dir_name, file, NULL);

        if ( g_file_test(full_filename, G_FILE_TEST_IS_DIR) )
        {
            g_free(full_filename);
            continue;
        }

        module = g_module_open(full_filename, G_MODULE_BIND_LAZY|G_MODULE_BIND_LOCAL);
        if ( module == NULL )
        {
            g_warning("Couldn’t load module '%s': %s", file, g_module_error());
            g_free(full_filename);
            continue;
        }
        g_free(full_filename);

        if ( ! g_module_symbol(module, "eventd_nd_backend_init", (void **)&backend_get_info) )
            continue;

#if DEBUG
        g_debug("Loading plugin '%s'", file);
#endif /* ! DEBUG */

        backend = g_new0(EventdNdBackend, 1);
        backend->module = module;
        backend_get_info(backend);

        backend->context = backend->init();

        context->backends = g_list_prepend(context->backends, backend);
    }
}

static void
_eventd_nd_backend_load(EventdNdContext *context)
{
    const gchar *env_base_dir;
    gchar *plugins_dir;

    if ( ! g_module_supported() )
    {
        g_warning("Couldn’t load plugins: %s", g_module_error());
        return;
    }

    env_base_dir = g_getenv("EVENTD_NOTIFICATION_BACKENDS_DIR");
    if ( env_base_dir != NULL )
    {
        if ( env_base_dir[0] == '~' )
            plugins_dir = g_build_filename(g_get_home_dir(), env_base_dir+2, NULL);
        else
            plugins_dir = g_build_filename(env_base_dir,  NULL);

        if ( g_file_test(plugins_dir, G_FILE_TEST_IS_DIR) )
            _eventd_nd_backends_load_dir(context, plugins_dir);
        g_free(plugins_dir);
    }

    plugins_dir = g_build_filename(g_get_user_data_dir(), PACKAGE_NAME, "plugins", "notification", NULL);
    if ( g_file_test(plugins_dir, G_FILE_TEST_IS_DIR) )
        _eventd_nd_backends_load_dir(context, plugins_dir);
    g_free(plugins_dir);

    plugins_dir = g_build_filename(DATADIR, PACKAGE_NAME, "plugins", "notification", NULL);
    if ( g_file_test(plugins_dir, G_FILE_TEST_IS_DIR) )
        _eventd_nd_backends_load_dir(context, plugins_dir);
    g_free(plugins_dir);

    plugins_dir = g_build_filename(LIBDIR, PACKAGE_NAME, "plugins", "notification", NULL);
    if ( g_file_test(plugins_dir, G_FILE_TEST_IS_DIR) )
        _eventd_nd_backends_load_dir(context, plugins_dir);
    g_free(plugins_dir);
}

EventdNdContext *
eventd_nd_init()
{
    EventdNdContext *context = NULL;

    context = g_new0(EventdNdContext, 1);

    eventd_nd_bubble_init();

    _eventd_nd_backend_load(context);

    context->style = eventd_nd_style_new();

    context->bubbles = g_hash_table_new_full(g_direct_hash, g_direct_equal, g_object_unref, eventd_nd_bubble_free);

    return context;
}

static void
_eventd_nd_backend_free(gpointer data)
{
    EventdNdBackend *backend = data;

    backend->uninit(backend->context);

    g_module_close(backend->module);

    g_free(backend);
}

static void
_eventd_nd_backend_display_free(gpointer data)
{
    EventdNdDisplayContext *display = data;

    display->backend->display_free(display->display);

    g_free(display);
}

void
eventd_nd_uninit(EventdNdContext *context)
{
    if ( context == NULL )
        return;

    g_hash_table_unref(context->bubbles);

    g_list_free_full(context->displays, _eventd_nd_backend_display_free);

    eventd_nd_style_free(context->style);

    g_list_free_full(context->backends, _eventd_nd_backend_free);

    eventd_nd_bubble_uninit();

    g_free(context);
}

void
eventd_nd_control_command(EventdNdContext *context, const gchar *command)
{
    const gchar *target = command+20;
    EventdNdDisplay *display;
    GList *backend_;

    if ( context == NULL )
        return;

    if ( ! g_str_has_prefix(command, "notification-daemon ") )
        return;

    for ( backend_ = context->backends ; backend_ != NULL ; backend_ = g_list_next(backend_) )
    {
        EventdNdBackend *backend = backend_->data;

        if ( ! backend->display_test(backend->context, target) )
            continue;

        display = backend->display_new(backend->context, target, eventd_nd_style_get_bubble_anchor(context->style), eventd_nd_style_get_bubble_margin(context->style));
        if ( display == NULL )
            g_warning("Couldn’t initialize display for '%s'", target);
        else
        {
            EventdNdDisplayContext *display_context;

            display_context = g_new(EventdNdDisplayContext, 1);
            display_context->backend = backend;
            display_context->display = display;

            context->displays = g_list_prepend(context->displays, display_context);
        }
    }
}

static void
_eventd_nd_event_ended(EventdEvent *event, EventdEventEndReason reason, EventdNdContext *context)
{
    EventdNdBubble *bubble;

    bubble = g_hash_table_lookup(context->bubbles, event);

    eventd_nd_bubble_hide(bubble);

    g_hash_table_remove(context->bubbles, event);
}

void
eventd_nd_event_action(EventdNdContext *context, EventdEvent *event, EventdNotificationNotification *notification)
{
    EventdNdBubble *bubble;

    if ( context == NULL )
        return;

    g_signal_connect(event, "ended", G_CALLBACK(_eventd_nd_event_ended), context);

    /*
     * TODO: Update an existing bubble
     */
    bubble = eventd_nd_bubble_new(event, notification, context->style, context->displays);

    eventd_nd_bubble_show(bubble);
    g_hash_table_insert(context->bubbles, g_object_ref(event), bubble);
}
