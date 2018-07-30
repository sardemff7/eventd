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
#include <glib.h>

#include "libeventd-helpers-config.h"

#include "types.h"

#include <wayland-server.h>
#include <compositor.h>
#include "notification-manager-unstable-v1-server-protocol.h"

typedef struct _EventdNdWestonQueue EventdNdWestonQueue;

typedef struct {
    struct weston_compositor *compositor;
    struct wl_resource *binding;
    struct weston_layer layer;
    struct wl_listener output_created_listener;
    struct wl_listener output_destroyed_listener;
    struct wl_listener output_moved_listener;
    struct wl_listener output_resized_listener;
    GHashTable *queues;
    EventdNdWestonQueue *default_queue;
} EventdNdWestonManager;

struct _EventdNdWestonQueue {
    EventdNdWestonManager *manager;
    gchar *name;
    struct wl_resource *resource;
    gchar **outputs;
    struct {
        gboolean right;
        gboolean center;
        gboolean bottom;
    } anchor;
    guint limit;
    struct weston_position margin;
    gint spacing;
    gboolean reverse;
    gboolean more_indicator;
    struct weston_output *output;
    gint32 scale;
    struct weston_geometry geometry;
    gboolean dirty_state;
    gboolean dirty_geometry;
    GQueue *queue;
    GQueue *wait_queue;
};

typedef struct {
    struct wl_resource *resource;
    EventdNdWestonQueue *queue;
    GList *link;
    struct weston_surface *surface;
    struct weston_geometry geometry, next_geometry;
    gboolean shown;
    struct weston_view *view;
    struct wl_listener view_destroy_listener;
} EventdNdWestonNotification;

static const gchar * const _eventd_nd_anchors[_EVENTD_ND_ANCHOR_SIZE] = {
    [EVENTD_ND_ANCHOR_TOP_LEFT]     = "top-left",
    [EVENTD_ND_ANCHOR_TOP]          = "top",
    [EVENTD_ND_ANCHOR_TOP_RIGHT]    = "top-right",
    [EVENTD_ND_ANCHOR_BOTTOM_LEFT]  = "bottom-left",
    [EVENTD_ND_ANCHOR_BOTTOM]       = "bottom",
    [EVENTD_ND_ANCHOR_BOTTOM_RIGHT] = "bottom-right",
};

static void
_eventd_nd_weston_request_destroy(struct wl_client *client, struct wl_resource *resource)
{
    wl_resource_destroy(resource);
}

static void
_eventd_nd_weston_notification_set_position(EventdNdWestonNotification *self, gint x, gint y, gboolean showing)
{
    struct weston_layer_entry *layer_entry;
    gint dx = self->view->geometry.x - x;
    gint dy = self->view->geometry.y - y;
    weston_view_set_position(self->view, x, y);
    weston_view_update_transform(self->view);
    weston_surface_damage(self->surface);

    gfloat sa = 1.0;
    if ( ! weston_view_is_mapped(self->view) )
    {
        weston_layer_entry_insert(wl_container_of(self->queue->manager->layer.view_list.link.prev, layer_entry, link), &self->view->layer_link);
        self->view->is_mapped = true;
        sa = 0.0;
    }
    else
    {
        if ( showing )
            sa = 0.5;
        weston_move_run(self->view, dx, dy, 0, 1, true, NULL, NULL);
    }
    if ( sa < 1.0 )
        weston_fade_run(self->view, sa, self->shown ? 1.0 : 0.5, 400.0, NULL, NULL);
}

static void
_eventd_nd_weston_notification_queue_update_more_stack(EventdNdWestonQueue *queue, gint wx, gint wy)
{
    GList *self_;
    for ( self_ = g_queue_peek_head_link(queue->wait_queue) ; self_ != NULL ; self_ = g_list_next(self_) )
    {
        EventdNdWestonNotification *self = self_->data;
        struct weston_geometry geometry = self->geometry;
        geometry.x *= queue->scale;
        geometry.y *= queue->scale;
        geometry.width *= queue->scale;
        geometry.height *= queue->scale;

        if ( queue->anchor.right )
            wx -= queue->spacing * queue->scale;
        else
            wx += queue->spacing * queue->scale;

        gint x, y;
        x = queue->anchor.center ? ( ( wx / 2 ) - ( geometry.width / 2 ) ) : queue->anchor.right ? ( wx - geometry.width ) : wx;
        y = queue->anchor.bottom ? ( wy - geometry.height ) : wy;
        x -= geometry.x;
        y -= geometry.y;

        _eventd_nd_weston_notification_set_position(self, x, y, FALSE);

        if ( queue->anchor.center )
            wy += queue->spacing * queue->scale;
    }
}

static void
_eventd_nd_weston_notification_queue_update(EventdNdWestonQueue *queue)
{
    if ( queue->limit > 0 )
    {
        while ( ( g_queue_get_length(queue->queue) < queue->limit ) && ( ! g_queue_is_empty(queue->wait_queue) ) )
        {
            GList *link;
            link = g_queue_pop_head_link(queue->wait_queue);
            if ( queue->reverse )
                g_queue_push_tail_link(queue->queue, link);
            else
                g_queue_push_head_link(queue->queue, link);
        }
    }

    gint bx, by;
    bx = 0;
    by = 0;
    if ( queue->anchor.right || queue->anchor.center )
        bx = queue->geometry.width;
    if ( queue->anchor.bottom )
        by = queue->geometry.height;
    bx += queue->geometry.x;
    by += queue->geometry.y;

    GList *self_;
    gint wx = bx;
    gint wy = by - queue->spacing * queue->scale;
    struct weston_geometry geometry = { .x = 0 };
    for ( self_ = g_queue_peek_head_link(queue->queue) ; self_ != NULL ; self_ = g_list_next(self_) )
    {
        if ( ! queue->anchor.bottom )
            wy += geometry.height + queue->spacing * queue->scale;
        EventdNdWestonNotification *self = self_->data;
        geometry = self->geometry;
        geometry.x *= queue->scale;
        geometry.y *= queue->scale;
        geometry.width *= queue->scale;
        geometry.height *= queue->scale;

        if ( queue->anchor.bottom )
            wy -= geometry.height + queue->spacing * queue->scale;

        gint x, y;
        x = queue->anchor.center ? ( ( wx / 2 ) - ( geometry.width / 2 ) ) : queue->anchor.right ? ( wx - geometry.width ) : bx;
        y = wy;
        x -= geometry.x;
        y -= geometry.y;

        gboolean showing = ! self->shown;
        if ( showing )
        {
            self->shown = TRUE;
            zeventd_nw_notification_v1_send_shown(self->resource);
        }
        _eventd_nd_weston_notification_set_position(self, x, y, showing);
    }

    if ( g_queue_is_empty(queue->wait_queue) )
        return;

    _eventd_nd_weston_notification_queue_update_more_stack(queue, wx, queue->reverse ? wy : by);
}

static void
_eventd_nd_weston_notification_committed(struct weston_surface *surface, int dx, int dy)
{
    EventdNdWestonNotification *self = surface->committed_private;

    self->geometry = self->next_geometry;
    _eventd_nd_weston_notification_queue_update(self->queue);
}

static void
_eventd_nd_weston_notification_request_set_geometry(struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height)
{
    EventdNdWestonNotification *self = wl_resource_get_user_data(resource);

    self->next_geometry.x = x;
    self->next_geometry.y = y;
    self->next_geometry.width = width;
    self->next_geometry.height = height;
}

static void
_eventd_nd_weston_notification_view_destroyed(struct wl_listener *listener, void *data)
{
    EventdNdWestonNotification *self = wl_container_of(listener, self, view_destroy_listener);

    wl_list_remove(&self->view_destroy_listener.link);
    weston_view_damage_below(self->view);
    self->view = NULL;
}

static const struct zeventd_nw_notification_v1_interface _eventd_nd_weston_notification_implementation = {
    .destroy = _eventd_nd_weston_request_destroy,
    .set_geometry = _eventd_nd_weston_notification_request_set_geometry,
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
    g_queue_delete_link(self->queue->queue, self->link);
    _eventd_nd_weston_notification_queue_update(self->queue);
}

static void
_eventd_nd_weston_queue_create_notification_request(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface_resource)
{
    EventdNdWestonQueue *queue = wl_resource_get_user_data(resource);
    struct weston_surface *surface = wl_resource_get_user_data(surface_resource);

    if ( weston_surface_set_role(surface, "eventd_nw_notification", resource, ZEVENTD_NW_NOTIFICATION_QUEUE_V1_ERROR_ROLE) < 0 )
        return;

    EventdNdWestonNotification *self;
    self = zalloc(sizeof(EventdNdWestonNotification));
    if ( self == NULL )
    {
        wl_resource_post_no_memory(surface_resource);
        return;
    }

    self->queue = queue;
    self->surface = surface;
    self->surface->committed = _eventd_nd_weston_notification_committed;
    self->surface->committed_private = self;

    self->view = weston_view_create(self->surface);
    if ( self->view == NULL )
    {
        wl_resource_post_no_memory(surface_resource);
        free(self);
        return;
    }

    self->resource = wl_resource_create(client, &zeventd_nw_notification_v1_interface, wl_resource_get_version(queue->resource), id);
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

    g_queue_push_tail(self->queue->wait_queue, self);
    self->link = g_queue_peek_tail_link(self->queue->wait_queue);
}

static const struct zeventd_nw_notification_queue_v1_interface _eventd_nd_weston_queue_implementation = {
    .destroy = _eventd_nd_weston_request_destroy,
    .create_notification = _eventd_nd_weston_queue_create_notification_request,
};

static void
_eventd_nd_weston_queue_send_state(EventdNdWestonQueue *self)
{
    if ( self->resource == NULL )
        return;

    if ( self->output == NULL )
    {
        if ( self->dirty_state )
            zeventd_nw_notification_queue_v1_send_inactive(self->resource);
    }
    else
    {
        if ( self->dirty_geometry || self->dirty_state )
            zeventd_nw_notification_queue_v1_send_geometry(self->resource, self->geometry.width, self->geometry.height, self->scale);
        if ( self->dirty_state )
            zeventd_nw_notification_queue_v1_send_active(self->resource);
    }
    self->dirty_geometry = self->dirty_state = FALSE;
}

static void
_eventd_nd_weston_queue_set_output(EventdNdWestonQueue *self, struct weston_output *output)
{
    gboolean check_geometry = FALSE;
    if ( output != NULL )
    {
        check_geometry = TRUE;
        if ( self->output == NULL )
            self->dirty_state = TRUE;
    }
    else if ( self->output != NULL )
        self->dirty_state = TRUE;

    self->output = output;

    gboolean moved = FALSE;
    if ( check_geometry )
    {
        struct weston_geometry geometry = {
            .x = self->output->x + self->margin.x * self->output->scale,
            .y = self->output->y + self->margin.y * self->output->scale,
            .width = self->output->width - 2 * self->margin.x * self->output->scale,
            .height = self->output->height - 2 * self->margin.y * self->output->scale,
        };
        if ( ( self->scale != self->output->scale ) || ( self->geometry.width != geometry.width ) || ( self->geometry.height == geometry.height ) )
            self->dirty_geometry = TRUE;

        moved = ( self->geometry.x != geometry.x ) || ( self->geometry.y != geometry.y );

        self->geometry = geometry;
        self->scale = self->output->scale;
    }

    _eventd_nd_weston_queue_send_state(self);
    if ( moved )
        _eventd_nd_weston_notification_queue_update(self);
}

static void
_eventd_nd_weston_queue_update_output(EventdNdWestonQueue *self)
{
    struct weston_output *output = NULL, *tmp;

    if ( wl_list_empty(&self->manager->compositor->output_list) )
    {
        if ( self->output != NULL )
            _eventd_nd_weston_queue_set_output(self, NULL);
        return;
    }

    gchar **output_name;
    if ( self->outputs != NULL )
    for ( output_name = self->outputs ; ( *output_name != NULL ) && ( output == NULL ) ; ++output_name )
    {
        wl_list_for_each(tmp, &self->manager->compositor->output_list, link)
        {
            if ( g_strcmp0(*output_name, tmp->name) == 0 )
            {
                output = tmp;
                break;
            }
        }
    }

    if ( output == NULL )
        output = wl_container_of(self->manager->compositor->output_list.next, output, link);

    if ( self->output != output )
        _eventd_nd_weston_queue_set_output(self, output);
}

static EventdNdWestonQueue *
_eventd_nd_weston_queue_new(EventdNdWestonManager *manager, const gchar *name)
{
    EventdNdWestonQueue *self;

    self = g_new0(EventdNdWestonQueue, 1);
    self->manager = manager;
    self->name = g_strdup(name);
    g_hash_table_insert(manager->queues, self->name, self);

    /* Defaults placement values */
    self->limit = 1;

    self->anchor.right = TRUE; /* top-right */
    self->margin.x = 13;
    self->margin.y = 13;
    self->spacing = 13;

    self->queue = g_queue_new();
    self->wait_queue = g_queue_new();

    return self;
}

static void
_eventd_nd_weston_queue_destroy(struct wl_resource *resource)
{
    EventdNdWestonQueue *queue = wl_resource_get_user_data(resource);

    queue->resource = NULL;
}

static void
_eventd_nd_weston_manager_get_queue_request(struct wl_client *client, struct wl_resource *resource, uint32_t id, const char *name)
{
    EventdNdWestonManager *manager = wl_resource_get_user_data(resource);
    EventdNdWestonQueue *self;

    if ( name == NULL )
        self = manager->default_queue;
    else
        self = g_hash_table_lookup(manager->queues, name);
    if ( self == NULL )
    {
        wl_resource_post_error(resource, ZEVENTD_NW_NOTIFICATION_MANAGER_V1_ERROR_UNKNOWN_QUEUE, "unknown queue %s", name);
        return;
    }

    if ( self->resource != NULL )
    {
        wl_resource_post_error(resource, ZEVENTD_NW_NOTIFICATION_MANAGER_V1_ERROR_DUPLICATE_QUEUE, "queue %s already created", name);
        return;
    }

    self->resource = wl_resource_create(client, &zeventd_nw_notification_queue_v1_interface, wl_resource_get_version(manager->binding), id);
    if ( self->resource == NULL )
    {
        wl_resource_post_no_memory(resource);
        return;
    }
    wl_resource_set_implementation(self->resource, &_eventd_nd_weston_queue_implementation, self, _eventd_nd_weston_queue_destroy);

    self->dirty_state = TRUE;
    _eventd_nd_weston_queue_send_state(self);
}

static void
_eventd_nd_weston_queue_delete(gpointer data)
{
    EventdNdWestonQueue *queue = data;

    if ( queue->resource != NULL )
        zeventd_nw_notification_queue_v1_send_deleted(queue->resource);

    g_free(queue);
}

static const struct zeventd_nw_notification_manager_v1_interface _eventd_nd_weston_manager_implementation = {
    .destroy = _eventd_nd_weston_request_destroy,
    .get_queue = _eventd_nd_weston_manager_get_queue_request,
};

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

    resource = wl_resource_create(client, &zeventd_nw_notification_manager_v1_interface, version, id);
    wl_resource_set_implementation(resource, &_eventd_nd_weston_manager_implementation, manager, _eventd_nd_weston_manager_unbind);

    if ( manager->binding != NULL )
    {
        wl_resource_post_error(resource, ZEVENTD_NW_NOTIFICATION_MANAGER_V1_ERROR_BOUND, "interface object already bound");
        wl_resource_destroy(resource);
        return;
    }

    manager->binding = resource;

    GHashTableIter iter;

    gchar *name;
    EventdNdWestonQueue *queue;
    g_hash_table_iter_init(&iter, manager->queues);
    while ( g_hash_table_iter_next(&iter, (gpointer *) &name, (gpointer *) &queue) )
    {
        if ( queue == manager->default_queue )
            continue;
        zeventd_nw_notification_manager_v1_send_queue(manager->binding, name);
    }
}

static void
_eventd_nd_weston_manager_output_created(struct wl_listener *listener, void *data)
{
    EventdNdWestonManager *manager = wl_container_of(listener, manager, output_created_listener);

    GHashTableIter iter;
    EventdNdWestonQueue *queue;
    g_hash_table_iter_init(&iter, manager->queues);
    while ( g_hash_table_iter_next(&iter, NULL, (gpointer *) &queue) )
        _eventd_nd_weston_queue_update_output(queue);
}

static void
_eventd_nd_weston_manager_output_destroyed(struct wl_listener *listener, void *data)
{
    EventdNdWestonManager *manager = wl_container_of(listener, manager, output_destroyed_listener);
    struct weston_output *output = data;

    GHashTableIter iter;
    EventdNdWestonQueue *queue;
    g_hash_table_iter_init(&iter, manager->queues);
    while ( g_hash_table_iter_next(&iter, NULL, (gpointer *) &queue) )
    {
        if ( queue->output == output )
            _eventd_nd_weston_queue_update_output(queue);
    }
}

static void
_eventd_nd_weston_manager_output_moved(struct wl_listener *listener, void *data)
{
    EventdNdWestonManager *manager = wl_container_of(listener, manager, output_moved_listener);
    struct weston_output *output = data;

    GHashTableIter iter;
    EventdNdWestonQueue *queue;
    g_hash_table_iter_init(&iter, manager->queues);
    while ( g_hash_table_iter_next(&iter, NULL, (gpointer *) &queue) )
    {
        if ( queue->output == output )
            _eventd_nd_weston_queue_set_output(queue, output);
    }
}

static void
_eventd_nd_weston_manager_output_resized(struct wl_listener *listener, void *data)
{
    EventdNdWestonManager *manager = wl_container_of(listener, manager, output_resized_listener);
    struct weston_output *output = data;

    GHashTableIter iter;
    EventdNdWestonQueue *queue;
    g_hash_table_iter_init(&iter, manager->queues);
    while ( g_hash_table_iter_next(&iter, NULL, (gpointer *) &queue) )
    {
        if ( queue->output == output )
            _eventd_nd_weston_queue_set_output(queue, output);
    }
}

static void
_eventd_nd_weston_config_parse(EventdNdWestonManager *manager, const gchar *path)
{
    GKeyFile *config_file;
    GError *error = NULL;

    if ( ! g_file_test(path, G_FILE_TEST_IS_REGULAR) )
        return;

    config_file = g_key_file_new();
    if ( ! g_key_file_load_from_file(config_file, path, G_KEY_FILE_NONE, &error) )
    {
        g_warning("Could not read config file: %s", error->message);
        g_clear_error(&error);
        g_key_file_free(config_file);
        return;
    }

    gchar **groups, **group;
    groups = g_key_file_get_groups(config_file, NULL);
    if ( groups == NULL )
        return;

    for ( group = groups ; *group != NULL ; ++group )
    {
        if ( ! g_str_has_prefix(*group, "Queue ") )
            continue;

        const gchar *name = *group + strlen("Queue ");
        EventdNdWestonQueue *self;

        self = g_hash_table_lookup(manager->queues, name);
        if ( self == NULL )
            self = _eventd_nd_weston_queue_new(manager, name);

        gchar **string_list;
        guint64 anchor;
        Int integer;
        Int integer_list[2];
        gsize length = 2;
        gboolean boolean;

        if ( evhelpers_config_key_file_get_string_list(config_file, *group, "Outputs", &string_list, NULL) == 0 )
        {
            g_strfreev(self->outputs);
            self->outputs = string_list;
        }

        if ( evhelpers_config_key_file_get_enum(config_file, *group, "Anchor", _eventd_nd_anchors, G_N_ELEMENTS(_eventd_nd_anchors), &anchor) == 0 )
        {
            self->anchor.right = ( anchor == EVENTD_ND_ANCHOR_TOP_RIGHT ) || ( anchor == EVENTD_ND_ANCHOR_BOTTOM_RIGHT );
            self->anchor.center = ( anchor == EVENTD_ND_ANCHOR_TOP ) || ( anchor == EVENTD_ND_ANCHOR_BOTTOM );
            self->anchor.bottom = ( anchor == EVENTD_ND_ANCHOR_BOTTOM_LEFT ) || ( anchor == EVENTD_ND_ANCHOR_BOTTOM ) || ( anchor == EVENTD_ND_ANCHOR_BOTTOM_RIGHT );
        }

        if ( evhelpers_config_key_file_get_int(config_file, *group, "Limit", &integer) == 0 )
            self->limit = ( integer.value > 0 ) ? integer.value : 0;

        if ( evhelpers_config_key_file_get_boolean(config_file, *group, "MoreIndicator", &boolean) == 0 )
            self->more_indicator = boolean;

        if ( evhelpers_config_key_file_get_int_list(config_file, *group, "Margin", integer_list, &length) == 0 )
        {
            switch ( length )
            {
            case 1:
                integer_list[1] = integer_list[0];
                /* fallthrough */
            case 2:
                self->margin.x = MAX(0, integer_list[0].value);
                self->margin.y = MAX(0, integer_list[1].value);
            break;
            }
        }

        if ( evhelpers_config_key_file_get_int(config_file, *group, "Spacing", &integer) == 0 )
            self->spacing = ( integer.value > 0 ) ? integer.value : 0;

        if ( evhelpers_config_key_file_get_boolean(config_file, *group, "OldestAtAnchor", &boolean) == 0 )
            self->reverse = boolean;
    }
    g_strfreev(groups);
    g_key_file_free(config_file);
}

EVENTD_EXPORT int
wet_module_init(struct weston_compositor *compositor, int *argc, char *argv[])
{
    EventdNdWestonManager *manager;

    manager = g_new0(EventdNdWestonManager, 1);
    manager->compositor = compositor;

    if ( wl_global_create(manager->compositor->wl_display, &zeventd_nw_notification_manager_v1_interface, 1, manager, _eventd_nd_weston_manager_bind) == NULL)
        return -1;

    manager->output_created_listener.notify = _eventd_nd_weston_manager_output_created;
    manager->output_destroyed_listener.notify = _eventd_nd_weston_manager_output_destroyed;
    manager->output_moved_listener.notify = _eventd_nd_weston_manager_output_moved;
    manager->output_resized_listener.notify = _eventd_nd_weston_manager_output_resized;
    wl_signal_add(&manager->compositor->output_created_signal, &manager->output_created_listener);
    wl_signal_add(&manager->compositor->output_destroyed_signal, &manager->output_destroyed_listener);
    wl_signal_add(&manager->compositor->output_moved_signal, &manager->output_moved_listener);
    wl_signal_add(&manager->compositor->output_resized_signal, &manager->output_resized_listener);

    weston_layer_init(&manager->layer, manager->compositor);
    weston_layer_set_position(&manager->layer, WESTON_LAYER_POSITION_UI);

    manager->queues = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, _eventd_nd_weston_queue_delete);
    manager->default_queue = _eventd_nd_weston_queue_new(manager, "default");

    gchar *path;
    path = g_build_filename(g_get_user_config_dir(), PACKAGE_NAME G_DIR_SEPARATOR_S PACKAGE_NAME ".conf", NULL);
    _eventd_nd_weston_config_parse(manager, path);
    g_free(path);

    GHashTableIter iter;
    EventdNdWestonQueue *queue;
    g_hash_table_iter_init(&iter, manager->queues);
    while ( g_hash_table_iter_next(&iter, NULL, (gpointer *) &queue) )
        _eventd_nd_weston_queue_update_output(queue);

    return 0;
}
