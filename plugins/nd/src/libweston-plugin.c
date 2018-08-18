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

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <linux/input.h>
#include <assert.h>
#include <signal.h>
#include <math.h>
#include <sys/types.h>

#include <wayland-server.h>
#include <compositor.h>
#include "notification-area-unstable-v1-server-protocol.h"

typedef struct {
    struct weston_compositor *compositor;
    struct wl_resource *binding;
    struct weston_layer layer;
    struct weston_output *output;
    struct weston_geometry workarea;
    struct wl_listener output_created_listener;
    struct wl_listener output_destroyed_listener;
    struct wl_listener output_moved_listener;
} EventdNdWestonManager;

typedef struct {
    struct wl_resource *resource;
    EventdNdWestonManager *manager;
    struct weston_surface *surface;
    struct weston_view *view;
    struct wl_listener view_destroy_listener;
} EventdNdWestonNotification;

static void
_eventd_nd_weston_request_destroy(struct wl_client *client, struct wl_resource *resource)
{
    wl_resource_destroy(resource);
}

static struct weston_output *
_eventd_nd_weston_get_default_output(EventdNdWestonManager *manager)
{
    if ( wl_list_empty(&manager->compositor->output_list) )
        return NULL;

    return wl_container_of(manager->compositor->output_list.next, manager->output, link);
}

static void
_eventd_nd_weston_notification_request_move(struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y)
{
    EventdNdWestonNotification *self = wl_resource_get_user_data(resource);
    int32_t dx, dy;

    x += self->manager->workarea.x;
    y += self->manager->workarea.y;
    dx = self->view->geometry.x - x;
    dy = self->view->geometry.y - y;

    weston_view_set_position(self->view, x, y);
    weston_view_update_transform(self->view);
    weston_surface_damage(self->surface);

    if ( ! weston_view_is_mapped(self->view) )
    {
        weston_layer_entry_insert(&self->manager->layer.view_list, &self->view->layer_link);
        self->view->is_mapped = true;
        weston_fade_run(self->view, 0, 1, 400.0, NULL, NULL);
    }
    else
        weston_move_run(self->view, dx, dy, 0, 1, true, NULL, NULL);
}

static void
_eventd_nd_weston_notification_view_destroyed(struct wl_listener *listener, void *data)
{
    EventdNdWestonNotification *self = wl_container_of(listener, self, view_destroy_listener);

    wl_list_remove(&self->view_destroy_listener.link);
    weston_view_damage_below(self->view);
    self->view = NULL;
}

static const struct zww_notification_v1_interface _eventd_nd_weston_notification_implementation = {
    .destroy = _eventd_nd_weston_request_destroy,
    .move = _eventd_nd_weston_notification_request_move,
};

static void
_eventd_nd_weston_notification_fade_out_done(struct weston_view_animation *animation, void *data)
{
    EventdNdWestonNotification *self = data;

    weston_surface_destroy(self->surface);
    free(self);
}

static void
_eventd_nd_weston_notification_destroy(struct wl_resource *resource)
{
    EventdNdWestonNotification *self = wl_resource_get_user_data(resource);

    weston_fade_run(self->view, 1, 0, 400.0, _eventd_nd_weston_notification_fade_out_done, self);
}

static void
_eventd_nd_weston_create_notification(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface_resource)
{
    EventdNdWestonManager *manager = wl_resource_get_user_data(resource);
    struct weston_surface *surface = wl_resource_get_user_data(surface_resource);

    if (weston_surface_set_role(surface, "ww_notification", resource, ZWW_NOTIFICATION_AREA_V1_ERROR_ROLE) < 0)
        return;

    EventdNdWestonNotification *self;
    self = zalloc(sizeof(EventdNdWestonNotification));
    if ( self == NULL )
    {
        wl_resource_post_no_memory(surface_resource);
        return;
    }

    self->manager = manager;
    self->surface = surface;

    self->view = weston_view_create(self->surface);
    if ( self->view == NULL )
    {
        wl_resource_post_no_memory(surface_resource);
        free(self);
        return;
    }

    self->resource = wl_resource_create(client, &zww_notification_v1_interface, 1, id);
    if ( self->resource == NULL )
    {
        wl_resource_post_no_memory(surface_resource);
        weston_view_destroy(self->view);
        free(self);
        return;
    }

    ++self->surface->ref_count;
    self->view_destroy_listener.notify = _eventd_nd_weston_notification_view_destroyed;
    wl_signal_add(&self->view->destroy_signal, &self->view_destroy_listener);
    wl_resource_set_implementation(self->resource, &_eventd_nd_weston_notification_implementation, self, _eventd_nd_weston_notification_destroy);
}

static const struct zww_notification_area_v1_interface _eventd_nd_weston_manager_implementation = {
    .destroy = _eventd_nd_weston_request_destroy,
    .create_notification = _eventd_nd_weston_create_notification,
};

static void
_eventd_nd_weston_set_output(EventdNdWestonManager *manager, struct weston_output *output)
{
    struct weston_geometry workarea = { 0, 0, 0, 0 };
    manager->output = output;
    if ( manager->output != NULL )
    {
        /* TODO: Support dock area API */
        {
            workarea.x = manager->output->x;
            workarea.y = manager->output->y;
            workarea.width = manager->output->width;
            workarea.height = manager->output->height;
        }
    }

    if ( ( manager->workarea.x == workarea.x ) && ( manager->workarea.y == workarea.y ) && ( manager->workarea.width == workarea.width ) && ( manager->workarea.height == workarea.height ) )
        return;

    manager->workarea = workarea;

    if ( manager->binding != NULL )
        zww_notification_area_v1_send_geometry(manager->binding, manager->workarea.width, manager->workarea.height, manager->output->current_scale);
}

static void
_eventd_nd_weston_manager_unbind(struct wl_resource *resource)
{
    EventdNdWestonManager *manager = wl_resource_get_user_data(resource);

    manager->binding = NULL;
}

static void
_eventd_nd_weston_manager_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
    EventdNdWestonManager *manager = data;
    struct wl_resource *resource;

    resource = wl_resource_create(client, &zww_notification_area_v1_interface, version, id);
    wl_resource_set_implementation(resource, &_eventd_nd_weston_manager_implementation, manager, _eventd_nd_weston_manager_unbind);

    if ( manager->binding != NULL )
    {
        wl_resource_post_error(resource, ZWW_NOTIFICATION_AREA_V1_ERROR_BOUND, "interface object already bound");
        wl_resource_destroy(resource);
        return;
    }

    manager->binding = resource;

    if ( manager->output == NULL )
        _eventd_nd_weston_set_output(manager, _eventd_nd_weston_get_default_output(manager));
    else
        zww_notification_area_v1_send_geometry(manager->binding, manager->workarea.width, manager->workarea.height, manager->output->current_scale);
}

static void
_eventd_nd_weston_manager_output_created(struct wl_listener *listener, void *data)
{
    EventdNdWestonManager *manager = wl_container_of(listener, manager, output_created_listener);

    if ( manager->output == NULL )
        _eventd_nd_weston_set_output(manager, _eventd_nd_weston_get_default_output(manager));
}

static void
_eventd_nd_weston_manager_output_destroyed(struct wl_listener *listener, void *data)
{
    EventdNdWestonManager *manager = wl_container_of(listener, manager, output_destroyed_listener);
    struct weston_output *output = data;

    if ( manager->output == output )
        _eventd_nd_weston_set_output(manager, _eventd_nd_weston_get_default_output(manager));
}

static void
_eventd_nd_weston_manager_output_moved(struct wl_listener *listener, void *data)
{
    EventdNdWestonManager *manager = wl_container_of(listener, manager, output_moved_listener);
    struct weston_output *output = data;

    if ( manager->output == output )
        _eventd_nd_weston_set_output(manager, manager->output);
}

EVENTD_EXPORT int
wet_module_init(struct weston_compositor *compositor, int *argc, char *argv[])
{
    EventdNdWestonManager *manager;

    manager = zalloc(sizeof(EventdNdWestonManager));
    if ( manager == NULL )
        return -1;

    manager->compositor = compositor;

    manager->output_created_listener.notify = _eventd_nd_weston_manager_output_created;
    manager->output_destroyed_listener.notify = _eventd_nd_weston_manager_output_destroyed;
    manager->output_moved_listener.notify = _eventd_nd_weston_manager_output_moved;
    wl_signal_add(&manager->compositor->output_created_signal, &manager->output_created_listener);
    wl_signal_add(&manager->compositor->output_destroyed_signal, &manager->output_destroyed_listener);
    wl_signal_add(&manager->compositor->output_moved_signal, &manager->output_moved_listener);

    if ( wl_global_create(manager->compositor->wl_display, &zww_notification_area_v1_interface, 1, manager, _eventd_nd_weston_manager_bind) == NULL)
        return -1;

    weston_layer_init(&manager->layer, manager->compositor);
    weston_layer_set_position(&manager->layer, WESTON_LAYER_POSITION_UI);

    return 0;
}
