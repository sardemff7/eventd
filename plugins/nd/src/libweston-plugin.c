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

struct weston_notification_area {
    struct weston_compositor *compositor;
    struct wl_resource *binding;
    struct weston_layer layer;
    struct weston_output *output;
    struct weston_geometry workarea;
    struct wl_listener output_created_listener;
    struct wl_listener output_destroyed_listener;
    struct wl_listener output_moved_listener;
};

struct weston_notification_area_notification {
    struct wl_resource *resource;
    struct weston_notification_area *na;
    struct weston_surface *surface;
    struct weston_view *view;
    struct wl_listener view_destroy_listener;
};

static void
_weston_notification_area_request_destroy(struct wl_client *client, struct wl_resource *resource)
{
    wl_resource_destroy(resource);
}

static struct weston_output *
_weston_notification_area_get_default_output(struct weston_notification_area *na)
{
    if ( wl_list_empty(&na->compositor->output_list) )
        return NULL;

    return wl_container_of(na->compositor->output_list.next, na->output, link);
}

static void
_weston_notification_area_notification_request_move(struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y)
{
    struct weston_notification_area_notification *self = wl_resource_get_user_data(resource);
    int32_t dx, dy;

    x += self->na->workarea.x;
    y += self->na->workarea.y;
    dx = self->view->geometry.x - x;
    dy = self->view->geometry.y - y;

    weston_view_set_position(self->view, x, y);
    weston_view_update_transform(self->view);
    weston_surface_damage(self->surface);

    if ( ! weston_view_is_mapped(self->view) )
    {
        weston_layer_entry_insert(&self->na->layer.view_list, &self->view->layer_link);
        self->view->is_mapped = true;
        weston_fade_run(self->view, 0, 1, 400.0, NULL, NULL);
    }
    else
        weston_move_run(self->view, dx, dy, 0, 1, true, NULL, NULL);
}

static void
_weston_notification_area_notification_view_destroyed(struct wl_listener *listener, void *data)
{
    struct weston_notification_area_notification *self = wl_container_of(listener, self, view_destroy_listener);

    wl_list_remove(&self->view_destroy_listener.link);
    weston_view_damage_below(self->view);
    self->view = NULL;
}

static const struct zww_notification_v1_interface weston_notification_area_notification_implementation = {
    .destroy = _weston_notification_area_request_destroy,
    .move = _weston_notification_area_notification_request_move,
};

static void
_weston_notification_area_notification_fade_out_done(struct weston_view_animation *animation, void *data)
{
    struct weston_notification_area_notification *self = data;

    weston_surface_destroy(self->surface);
    free(self);
}

static void
_weston_notification_area_notification_destroy(struct wl_resource *resource)
{
    struct weston_notification_area_notification *self = wl_resource_get_user_data(resource);

    weston_fade_run(self->view, 1, 0, 400.0, _weston_notification_area_notification_fade_out_done, self);
}

static void
_weston_notification_area_create_notification(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface_resource)
{
    struct weston_notification_area *na = wl_resource_get_user_data(resource);
    struct weston_surface *surface = wl_resource_get_user_data(surface_resource);

    if (weston_surface_set_role(surface, "ww_notification", resource, ZWW_NOTIFICATION_AREA_V1_ERROR_ROLE) < 0)
        return;

    struct weston_notification_area_notification *self;
    self = zalloc(sizeof(struct weston_notification_area_notification));
    if ( self == NULL )
    {
        wl_resource_post_no_memory(surface_resource);
        return;
    }

    self->na = na;
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
    self->view_destroy_listener.notify = _weston_notification_area_notification_view_destroyed;
    wl_signal_add(&self->view->destroy_signal, &self->view_destroy_listener);
    wl_resource_set_implementation(self->resource, &weston_notification_area_notification_implementation, self, _weston_notification_area_notification_destroy);
}

static const struct zww_notification_area_v1_interface weston_notification_area_implementation = {
    .destroy = _weston_notification_area_request_destroy,
    .create_notification = _weston_notification_area_create_notification,
};

static void
_weston_notification_area_set_output(struct weston_notification_area *na, struct weston_output *output)
{
    struct weston_geometry workarea = { 0, 0, 0, 0 };
    na->output = output;
    if ( na->output != NULL )
    {
        /* TODO: Support dock area API */
        {
            workarea.x = na->output->x;
            workarea.y = na->output->y;
            workarea.width = na->output->width;
            workarea.height = na->output->height;
        }
    }

    if ( ( na->workarea.x == workarea.x ) && ( na->workarea.y == workarea.y ) && ( na->workarea.width == workarea.width ) && ( na->workarea.height == workarea.height ) )
        return;

    na->workarea = workarea;

    if ( na->binding != NULL )
        zww_notification_area_v1_send_geometry(na->binding, na->workarea.width, na->workarea.height, na->output->current_scale);
}

static void
_weston_notification_area_unbind(struct wl_resource *resource)
{
    struct weston_notification_area *na = wl_resource_get_user_data(resource);

    na->binding = NULL;
}

static void
_weston_notification_area_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
    struct weston_notification_area *na = data;
    struct wl_resource *resource;

    resource = wl_resource_create(client, &zww_notification_area_v1_interface, version, id);
    wl_resource_set_implementation(resource, &weston_notification_area_implementation, na, _weston_notification_area_unbind);

    if ( na->binding != NULL )
    {
        wl_resource_post_error(resource, ZWW_NOTIFICATION_AREA_V1_ERROR_BOUND, "interface object already bound");
        wl_resource_destroy(resource);
        return;
    }

    na->binding = resource;

    if ( na->output == NULL )
        _weston_notification_area_set_output(na, _weston_notification_area_get_default_output(na));
    else
        zww_notification_area_v1_send_geometry(na->binding, na->workarea.width, na->workarea.height, na->output->current_scale);
}

static void
_weston_notification_area_output_created(struct wl_listener *listener, void *data)
{
    struct weston_notification_area *na = wl_container_of(listener, na, output_created_listener);

    if ( na->output == NULL )
        _weston_notification_area_set_output(na, _weston_notification_area_get_default_output(na));
}

static void
_weston_notification_area_output_destroyed(struct wl_listener *listener, void *data)
{
    struct weston_notification_area *na = wl_container_of(listener, na, output_destroyed_listener);
    struct weston_output *output = data;

    if ( na->output == output )
        _weston_notification_area_set_output(na, _weston_notification_area_get_default_output(na));
}

static void
_weston_notification_area_output_moved(struct wl_listener *listener, void *data)
{
    struct weston_notification_area *na = wl_container_of(listener, na, output_moved_listener);
    struct weston_output *output = data;

    if ( na->output == output )
        _weston_notification_area_set_output(na, na->output);
}

EVENTD_EXPORT int
wet_module_init(struct weston_compositor *compositor, int *argc, char *argv[])
{
    struct weston_notification_area *na;

    na = zalloc(sizeof(struct weston_notification_area));
    if ( na == NULL )
        return -1;

    na->compositor = compositor;

    na->output_created_listener.notify = _weston_notification_area_output_created;
    na->output_destroyed_listener.notify = _weston_notification_area_output_destroyed;
    na->output_moved_listener.notify = _weston_notification_area_output_moved;
    wl_signal_add(&na->compositor->output_created_signal, &na->output_created_listener);
    wl_signal_add(&na->compositor->output_destroyed_signal, &na->output_destroyed_listener);
    wl_signal_add(&na->compositor->output_moved_signal, &na->output_moved_listener);

    if ( wl_global_create(na->compositor->wl_display, &zww_notification_area_v1_interface, 1, na, _weston_notification_area_bind) == NULL)
        return -1;

    weston_layer_init(&na->layer, na->compositor);
    weston_layer_set_position(&na->layer, WESTON_LAYER_POSITION_UI);

    return 0;
}
