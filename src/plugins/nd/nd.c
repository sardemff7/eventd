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
#include <gio/gio.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <eventd-plugin.h>
#include <libeventd-event.h>
#include <libeventd-config.h>

#include <eventd-nd-types.h>
#include <eventd-nd-backend.h>
#include <eventd-nd-notification.h>
#include <eventd-nd-style.h>

struct _EventdPluginContext {
    EventdNdInterface interface;
    GHashTable *events;
    EventdNdCornerAnchor bubble_anchor;
    gint bubble_margin;
    gint max_width;
    gint max_height;
    EventdNdStyle *style;
    GHashTable *backends;
    GList *displays;
    GHashTable *surfaces;
};

typedef struct {
    EventdNdBackend *backend;
    EventdNdDisplay *display;
} EventdNdDisplayContext;

typedef struct {
    EventdNdBackend *backend;
    EventdNdSurface *surface;
} EventdNdSurfaceContext;

typedef struct {
    gchar *title;
    gchar *message;
    gchar *image;
    gchar *icon;
    EventdNdStyle *style;
} EventdNdEvent;

static void
_eventd_nd_backends_load_dir(EventdPluginContext *context, const gchar *backends_dir_name, gchar **whitelist, gchar **blacklist)
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
        g_warning("Couldn't read the backends directory: %s", error->message);
        g_clear_error(&error);
        return;
    }

    while ( ( file = g_dir_read_name(plugins_dir) ) != NULL )
    {
        gchar *full_filename;
        EventdNdBackend *backend;
        const gchar **id;
        EventdNdBackendGetInfoFunc get_info;
        GModule *module;

        if ( ! g_str_has_suffix(file, G_MODULE_SUFFIX) )
            continue;

        if ( whitelist != NULL )
        {
            gboolean whitelisted = FALSE;
            gchar **wname;
            for ( wname = whitelist ; *wname != NULL ; ++wname )
            {
                if ( g_str_has_prefix(file, *wname) )
                {
                    whitelisted = TRUE;
                    break;
                }
            }
            if ( ! whitelisted )
                continue;
        }

        if ( blacklist != NULL )
        {
            gboolean blacklisted = FALSE;
            gchar **bname;
            for ( bname = blacklist ; *bname != NULL ; ++bname )
            {
                if ( g_str_has_prefix(file, *bname) )
                {
                    blacklisted = TRUE;
                    break;
                }
            }
            if ( blacklisted )
                continue;
        }

        full_filename = g_build_filename(backends_dir_name, file, NULL);

        if ( g_file_test(full_filename, G_FILE_TEST_IS_DIR) )
        {
            g_free(full_filename);
            continue;
        }

        module = g_module_open(full_filename, G_MODULE_BIND_LAZY|G_MODULE_BIND_LOCAL);
        if ( module == NULL )
        {
            g_warning("Couldn't load module '%s': %s", file, g_module_error());
            g_free(full_filename);
            continue;
        }
        g_free(full_filename);

        if ( ! g_module_symbol(module, "eventd_nd_backend_id", (void **)&id) )
            continue;

        if ( id == NULL )
        {
            g_warning("Backend '%s' must define eventd_nd_backend_id", file);
            continue;
        }

        if ( g_hash_table_lookup(context->backends, *id) != NULL )
        {
#if DEBUG
            g_debug("Backend '%s' with id '%s' already loaded", file, *id);
#endif /* ! DEBUG */
            continue;
        }

        if ( ! g_module_symbol(module, "eventd_nd_backend_get_info", (void **)&get_info) )
            continue;

#if DEBUG
        g_debug("Loading backend '%s': %s", file, *id);
#endif /* ! DEBUG */

        backend = g_new0(EventdNdBackend, 1);
        backend->module = module;
        get_info(backend);

        backend->context = backend->init(context, &context->interface);

        g_hash_table_insert(context->backends, g_strdup(*id), backend);
    }
}

static void
_eventd_nd_backend_load(EventdPluginContext *context)
{
    const gchar *env_whitelist;
    const gchar *env_blacklist;
    const gchar *env_base_dir;
    gchar **whitelist = NULL;
    gchar **blacklist = NULL;
    gchar *plugins_dir;

    if ( ! g_module_supported() )
    {
        g_warning("Couldn't load plugins: %s", g_module_error());
        return;
    }

    env_whitelist = g_getenv("EVENTD_NOTIFICATION_BACKENDS_WHITELIST");
    if ( env_whitelist != NULL )
        whitelist = g_strsplit(env_whitelist, ",", 0);

    env_blacklist = g_getenv("EVENTD_NOTIFICATION_BACKENDS_BLACKLIST");
    if ( env_blacklist != NULL )
        blacklist = g_strsplit(env_blacklist, ",", 0);

    env_base_dir = g_getenv("EVENTD_NOTIFICATION_BACKENDS_DIR");
    if ( env_base_dir != NULL )
    {
        if ( env_base_dir[0] == '~' )
            plugins_dir = g_build_filename(g_get_home_dir(), env_base_dir+2, NULL);
        else
            plugins_dir = g_build_filename(env_base_dir,  NULL);

        if ( g_file_test(plugins_dir, G_FILE_TEST_IS_DIR) )
            _eventd_nd_backends_load_dir(context, plugins_dir, whitelist, blacklist);
        g_free(plugins_dir);
    }

    plugins_dir = g_build_filename(g_get_user_data_dir(), PACKAGE_NAME, "plugins", "nd", NULL);
    if ( g_file_test(plugins_dir, G_FILE_TEST_IS_DIR) )
        _eventd_nd_backends_load_dir(context, plugins_dir, whitelist, blacklist);
    g_free(plugins_dir);

    plugins_dir = g_build_filename(DATADIR, PACKAGE_NAME, "plugins", "nd", NULL);
    if ( g_file_test(plugins_dir, G_FILE_TEST_IS_DIR) )
        _eventd_nd_backends_load_dir(context, plugins_dir, whitelist, blacklist);
    g_free(plugins_dir);

    plugins_dir = g_build_filename(LIBDIR, PACKAGE_NAME, "plugins", "nd", NULL);
    if ( g_file_test(plugins_dir, G_FILE_TEST_IS_DIR) )
        _eventd_nd_backends_load_dir(context, plugins_dir, whitelist, blacklist);
    g_free(plugins_dir);
}

static void
_eventd_nd_get_max_width_and_height(gint width, gint height, EventdNdStyle *style, gint *new_width, gint *new_height)
{
    gint w, h;

    w = eventd_nd_style_get_image_max_width(style);
    h = eventd_nd_style_get_image_max_height(style);

    width = MAX(width, w);
    height = MAX(height, h);

    w = eventd_nd_style_get_icon_max_width(style);
    h = eventd_nd_style_get_icon_max_height(style);

    width = MAX(width, w);
    height = MAX(height, h);

    *new_width = width;
    *new_height = height;
}

static void
_eventd_nd_event_update(EventdNdEvent *event, GKeyFile *config_file)
{
    gchar *string;

    if ( libeventd_config_key_file_get_string(config_file, "Notification", "Title", &string) == 0 )
    {
        g_free(event->title);
        event->title = string;
    }
    if ( libeventd_config_key_file_get_string(config_file, "Notification", "Message", &string) == 0 )
    {
        g_free(event->message);
        event->message = string;
    }
    if ( libeventd_config_key_file_get_string(config_file, "Notification", "Image", &string) == 0 )
    {
        g_free(event->image);
        event->image = string;
    }
    if ( libeventd_config_key_file_get_string(config_file, "Notification", "Icon", &string) == 0 )
    {
        g_free(event->icon);
        event->icon = string;
    }

    eventd_nd_style_update(event->style, config_file);
}

static EventdNdEvent *
_eventd_nd_event_new(EventdPluginContext *context)
{
    EventdNdEvent *event = NULL;

    event = g_new0(EventdNdEvent, 1);

    event->title = g_strdup("$name");
    event->message = g_strdup("$text");
    event->image = g_strdup("image");
    event->icon = g_strdup("icon");
    event->style = eventd_nd_style_new(context->style);

    return event;
}

static void
_eventd_nd_event_free(gpointer data)
{
    EventdNdEvent *event = data;

    g_free(event->image);
    g_free(event->icon);
    g_free(event->message);
    g_free(event->title);
    g_free(event);
}

static void
_eventd_nd_event_parse(EventdPluginContext *context, const gchar *id, GKeyFile *config_file)
{
    gboolean disable;
    EventdNdEvent *event = NULL;

    if ( ! g_key_file_has_group(config_file, "Notification") )
        return;

    if ( libeventd_config_key_file_get_boolean(config_file, "Notification", "Disable", &disable) < 0 )
        return;

    if ( ! disable )
    {
        event = _eventd_nd_event_new(context);
        _eventd_nd_event_update(event, config_file);
        _eventd_nd_get_max_width_and_height(context->max_width, context->max_height, event->style, &context->max_width, &context->max_height);
    }

    g_hash_table_insert(context->events, g_strdup(id), event);
}

static void
_eventd_nd_surface_hide(gpointer data)
{
    EventdNdSurfaceContext *surface = data;

    surface->backend->surface_hide(surface->surface);

    g_free(surface);
}

static void
_eventd_nd_surface_hide_all(gpointer data)
{
    GList *surfaces = data;

    g_list_free_full(surfaces, _eventd_nd_surface_hide);
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

static void
_eventd_nd_backend_remove_display(EventdNdContext *context, EventdNdDisplay *display)
{
    EventdNdDisplayContext *display_context;
    GList *display_context_;
    for ( display_context_ = context->displays ; display_context_ != NULL ; display_context_ = g_list_next(display_context_) )
    {
        display_context = display_context_->data;
        if ( display_context->display == display )
        {
            _eventd_nd_backend_display_free(display_context);
            context->displays = g_list_delete_link(context->displays, display_context_);
            return;
        }
    }
}

static EventdPluginContext *
_eventd_nd_init(EventdCoreContext *core, EventdCoreInterface *interface)
{
    EventdPluginContext *context;

    context = g_new0(EventdPluginContext, 1);

    context->interface.remove_display = _eventd_nd_backend_remove_display;

    context->backends = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, _eventd_nd_backend_free);

    _eventd_nd_backend_load(context);

    context->events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, _eventd_nd_event_free);
    context->surfaces = g_hash_table_new_full(g_direct_hash, g_direct_equal, g_object_unref, _eventd_nd_surface_hide_all);

    eventd_nd_notification_init();

    /* default bubble position */
    context->bubble_anchor    = EVENTD_ND_ANCHOR_TOP_RIGHT;
    context->bubble_margin    = 13;

    context->style = eventd_nd_style_new(NULL);

    return context;
}

static void
_eventd_nd_uninit(EventdPluginContext *context)
{
    eventd_nd_style_free(context->style);

    eventd_nd_notification_uninit();

    g_hash_table_unref(context->surfaces);
    g_hash_table_unref(context->events);

    g_list_free_full(context->displays, _eventd_nd_backend_display_free);

    g_hash_table_unref(context->backends);

    g_free(context);
}

static void
_eventd_nd_control_command(EventdPluginContext *context, const gchar *command)
{
    const gchar *target = command+20;
    EventdNdDisplay *display;
    GHashTableIter iter;
    const gchar *id;
    EventdNdBackend *backend;

    if ( ! g_str_has_prefix(command, "notification-daemon ") )
        return;

    g_hash_table_iter_init(&iter, context->backends);
    while ( g_hash_table_iter_next(&iter, (gpointer *)&id, (gpointer *)&backend) )
    {
        if ( ! backend->display_test(backend->context, target) )
            continue;

        display = backend->display_new(backend->context, target, context->bubble_anchor, context->bubble_margin);
        if ( display == NULL )
            g_warning("Couldn't initialize display for '%s'", target);
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
_eventd_nd_global_parse(EventdPluginContext *context, GKeyFile *config_file)
{
    if ( g_key_file_has_group(config_file, "Notification") )
    {
        Int integer;
        gchar *string;

        if ( libeventd_config_key_file_get_string(config_file, "Notification", "Anchor", &string) == 0 )
        {
            if ( g_strcmp0(string, "top left") == 0 )
                context->bubble_anchor = EVENTD_ND_ANCHOR_TOP_LEFT;
            else if ( g_strcmp0(string, "top right") == 0 )
                context->bubble_anchor = EVENTD_ND_ANCHOR_TOP_RIGHT;
            else if ( g_strcmp0(string, "bottom left") == 0 )
                context->bubble_anchor = EVENTD_ND_ANCHOR_BOTTOM_LEFT;
            else if ( g_strcmp0(string, "bottom right") == 0 )
                context->bubble_anchor = EVENTD_ND_ANCHOR_BOTTOM_RIGHT;
            else
                g_warning("Wrong anchor value '%s'", string);
            g_free(string);
        }

        if ( libeventd_config_key_file_get_int(config_file, "Notification", "Margin", &integer) == 0 )
            context->bubble_margin = integer.value;
    }

    eventd_nd_style_update(context->style, config_file);
    _eventd_nd_get_max_width_and_height(context->max_width, context->max_height, context->style, &context->max_width, &context->max_height);
}

static void
_eventd_nd_event_updated(EventdEvent *event, EventdPluginContext *context)
{
    EventdNdEvent *nd_event;

    nd_event = g_hash_table_lookup(context->events, eventd_event_get_config_id(event));
    if ( nd_event == NULL )
        return;

    EventdNdNotification *notification;

    notification = eventd_nd_notification_new(event, nd_event->title, nd_event->message, nd_event->image, nd_event->icon, context->max_width, context->max_height);

    GList *surfaces = g_hash_table_lookup(context->surfaces, event);
    for ( ; surfaces != NULL ; surfaces = g_list_next(surfaces) )
    {
        EventdNdSurfaceContext *surface = surfaces->data;

        if ( surface->backend->surface_update != NULL )
            surface->surface = surface->backend->surface_update(surface->surface, notification, nd_event->style);
    }
}

static void
_eventd_nd_event_ended(EventdEvent *event, EventdEventEndReason reason, EventdPluginContext *context)
{
    g_hash_table_remove(context->surfaces, event);
}

static void
_eventd_nd_event_action(EventdPluginContext *context, EventdEvent *event)
{
    EventdNdEvent *nd_event;
    EventdNdNotification *notification;
    GList *display_;
    GList *surfaces = NULL;

    nd_event = g_hash_table_lookup(context->events, eventd_event_get_config_id(event));
    if ( nd_event == NULL )
        return;

    notification = eventd_nd_notification_new(event, nd_event->title, nd_event->message, nd_event->image, nd_event->icon, context->max_width, context->max_height);

    for ( display_ = context->displays ; display_ != NULL ; display_ = g_list_next(display_) )
    {
        EventdNdDisplayContext *display = display_->data;
        EventdNdSurface *surface;
        surface = display->backend->surface_show(event, display->display, notification, nd_event->style);
        if ( surface != NULL )
        {
            EventdNdSurfaceContext *surface_context;

            surface_context = g_new(EventdNdSurfaceContext, 1);
            surface_context->backend = display->backend;
            surface_context->surface = surface;

            surfaces = g_list_prepend(surfaces, surface_context);
        }
    }

    if ( surfaces != NULL )
    {
        g_signal_connect(event, "updated", G_CALLBACK(_eventd_nd_event_updated), context);
        g_signal_connect(event, "ended", G_CALLBACK(_eventd_nd_event_ended), context);

        g_hash_table_insert(context->surfaces, g_object_ref(event), surfaces);
    }

    eventd_nd_notification_free(notification);
}

static void
_eventd_nd_config_reset(EventdPluginContext *context)
{
    g_hash_table_remove_all(context->events);
}

const gchar *eventd_plugin_id = "eventd-nd";
void
eventd_plugin_get_info(EventdPlugin *plugin)
{
    plugin->init = _eventd_nd_init;
    plugin->uninit = _eventd_nd_uninit;

    plugin->control_command = _eventd_nd_control_command;

    plugin->config_reset = _eventd_nd_config_reset;

    plugin->global_parse = _eventd_nd_global_parse;
    plugin->event_parse = _eventd_nd_event_parse;
    plugin->event_action = _eventd_nd_event_action;
}

