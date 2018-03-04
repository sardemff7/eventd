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

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#ifdef __linux__
#include <linux/input.h>
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <glib-object.h>

#include <cairo.h>

#include <wayland-client.h>
#include <wayland-cursor.h>
#include "notification-area-unstable-v1-client-protocol.h"
#include <libgwater-wayland.h>

#include "libeventd-event.h"
#include "libeventd-helpers-config.h"

#include "backend.h"

/* Supported interface versions */
#define WL_COMPOSITOR_INTERFACE_VERSION 3
#define WW_NOTIFICATION_AREA_INTERFACE_VERSION 1
#define WL_SHM_INTERFACE_VERSION 1
#define WL_SEAT_INTERFACE_VERSION 5

typedef enum {
    WL_FORMAT_RGB  = 1 << 0,
    WL_FORMAT_ARGB = 1 << 1
} EventdNdWlFormats;

typedef struct {
    EventdNdBackendContext *context;
    NkBindingsSeat *bindings_seat;
    GSList *link;
    uint32_t global_name;
    struct wl_seat *seat;
    struct wl_keyboard *keyboard;
    struct wl_pointer *pointer;
    EventdNdSurface *surface;
    gint32 scroll[NK_BINDINGS_SCROLL_NUM_AXIS];
} EventdNdWlSeat;

typedef enum {
    EVENTD_ND_WL_GLOBAL_COMPOSITOR,
    EVENTD_ND_WL_GLOBAL_NOTIFICATION_DAEMON,
    EVENTD_ND_WL_GLOBAL_SHM,
    _EVENTD_ND_WL_GLOBAL_SIZE,
} EventdNdWlGlobalName;

struct _EventdNdBackendContext {
    EventdNdInterface *nd;
    NkBindings *bindings;
    GWaterWaylandSource *source;
    gboolean scale_support;
    struct wl_display *display;
    struct wl_registry *registry;
    uint32_t global_names[_EVENTD_ND_WL_GLOBAL_SIZE];
    struct wl_compositor *compositor;
    struct zww_notification_area_v1 *notification_area;
    struct wl_shm *shm;
    struct {
        gchar *theme_name;
        gchar **name;
        struct wl_cursor_theme *theme;
        struct wl_cursor *cursor;
        struct wl_cursor_image *image;
        struct wl_surface *surface;
        struct wl_callback *frame_cb;
    } cursor;
    GSList *seats;
    EventdNdWlFormats formats;
    gint32 scale;
    gboolean need_redraw;
};

typedef struct {
    gboolean released;
    struct wl_buffer *buffer;
    gpointer data;
    gsize size;
} EventdNdWlBuffer;

struct _EventdNdSurface {
    EventdNdNotification *notification;
    EventdNdBackendContext *context;
    EventdNdWlBuffer *buffer;
    gint width;
    gint height;
    struct wl_surface *surface;
    struct zww_notification_v1 *ww_notification;
};

static const NkBindingsButtonState _eventd_nd_wl_nk_bindings_button_state[] = {
    [WL_POINTER_BUTTON_STATE_PRESSED] = NK_BINDINGS_BUTTON_STATE_PRESS,
    [WL_POINTER_BUTTON_STATE_RELEASED] = NK_BINDINGS_BUTTON_STATE_RELEASE,
};

static const NkBindingsScrollAxis _eventd_nd_wl_nk_bindings_scroll_axis[] = {
    [WL_POINTER_AXIS_VERTICAL_SCROLL] = NK_BINDINGS_SCROLL_AXIS_VERTICAL,
    [WL_POINTER_AXIS_HORIZONTAL_SCROLL] = NK_BINDINGS_SCROLL_AXIS_HORIZONTAL,
};

static EventdNdBackendContext *
_eventd_nd_wl_init(EventdNdInterface *nd, NkBindings *bindings)
{
    EventdNdBackendContext *self;

    self = g_new0(EventdNdBackendContext, 1);

    self->nd = nd;
    self->bindings = bindings;

    return self;
}

static void
_eventd_nd_wl_uninit(EventdNdBackendContext *self)
{
    g_free(self);
}

static void
_eventd_nd_wl_global_parse(EventdNdBackendContext *self, GKeyFile *config_file)
{
    if ( ! g_key_file_has_group(config_file, "NotificationWayland") )
        return;

    evhelpers_config_key_file_get_string(config_file, "NotificationWayland", "CursorTheme", &self->cursor.theme_name);
    evhelpers_config_key_file_get_string_list(config_file, "NotificationWayland", "Cursor", &self->cursor.name, NULL);
}

static void
_eventd_nd_wl_config_reset(EventdNdBackendContext *self)
{
    g_free(self->cursor.theme_name);
    self->cursor.theme_name = NULL;
}

static void
_eventd_nd_wl_notification_area_geometry(void *data, struct zww_notification_area_v1 *notification_area, int32_t width, int32_t height, int32_t scale)
{
    EventdNdBackendContext *self = data;

#if CAIRO_VERSION >= CAIRO_VERSION_ENCODE(1, 14, 0)
    if ( self->scale_support )
    {
        self->need_redraw = ( self->scale != scale );
        self->scale = scale;
    }
#endif /* CAIRO_VERION >= 1.14.0 */

    self->nd->geometry_update(self->nd->context, width, height, self->scale);
}

static const struct zww_notification_area_v1_listener _eventd_nd_wl_notification_area_listener = {
    .geometry = _eventd_nd_wl_notification_area_geometry
};

static void
_eventd_nd_wl_shm_format(void *data, struct wl_shm *wl_shm, uint32_t format)
{
    EventdNdBackendContext *self = data;

    switch ( format )
    {
    case WL_SHM_FORMAT_XRGB8888:
        self->formats |= WL_FORMAT_RGB;
    break;
    case WL_SHM_FORMAT_ARGB8888:
        self->formats |= WL_FORMAT_ARGB;
    break;
    }
}

static const struct wl_shm_listener _eventd_nd_wl_shm_listener = {
    .format = _eventd_nd_wl_shm_format
};

static void
_eventd_nd_wl_keyboard_keymap(void *data, struct wl_keyboard *keyboard, enum wl_keyboard_keymap_format format, int32_t fd, uint32_t size)
{
    EventdNdWlSeat *self = data;

    if ( format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 )
    {
        close(fd);
        return;
    }

    gchar *keymap_str;
    keymap_str = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if ( keymap_str == MAP_FAILED )
    {
        g_warning("mmap failed: %s", g_strerror(errno));
        g_close(fd, NULL);
        return;
    }

    struct xkb_keymap *keymap;
    keymap = xkb_keymap_new_from_string(nk_bindings_seat_get_context(self->bindings_seat), keymap_str, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if ( keymap == NULL )
        goto fail;

    struct xkb_state *state;
    state = xkb_state_new(keymap);
    if ( state == NULL )
    {
        xkb_keymap_unref(keymap);
        goto fail;
    }

    nk_bindings_seat_update_keymap(self->bindings_seat, keymap, state);

    xkb_state_unref(state);
    xkb_keymap_unref(keymap);

fail:
    munmap(keymap_str, size);

    g_close(fd, NULL);
}

static void
_eventd_nd_wl_keyboard_enter(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys)
{
}

static void
_eventd_nd_wl_keyboard_leave(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface)
{
    EventdNdWlSeat *self = data;

    nk_bindings_seat_reset(self->bindings_seat);
}

static void
_eventd_nd_wl_keyboard_key(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t key, enum wl_keyboard_key_state state)
{
}

static void
_eventd_nd_wl_keyboard_modifiers(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
    EventdNdWlSeat *self = data;

    nk_bindings_seat_update_mask(self->bindings_seat, NULL, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

static void
_eventd_nd_wl_keyboard_repeat_info(void *data, struct wl_keyboard *keyboard, int32_t rate, int32_t delay)
{
}

static const struct wl_keyboard_listener _eventd_nd_wl_keyboard_listener = {
    .keymap = _eventd_nd_wl_keyboard_keymap,
    .enter = _eventd_nd_wl_keyboard_enter,
    .leave = _eventd_nd_wl_keyboard_leave,
    .key = _eventd_nd_wl_keyboard_key,
    .modifiers = _eventd_nd_wl_keyboard_modifiers,
    .repeat_info = _eventd_nd_wl_keyboard_repeat_info,
};

static void
_eventd_nd_cursor_set_image(EventdNdBackendContext *self, int i)
{
    struct wl_buffer *buffer;
    struct wl_cursor_image *image;
    image = self->cursor.cursor->images[i];

    self->cursor.image = image;
    buffer = wl_cursor_image_get_buffer(self->cursor.image);
    wl_surface_attach(self->cursor.surface, buffer, 0, 0);
    wl_surface_damage(self->cursor.surface, 0, 0, self->cursor.image->width, self->cursor.image->height);
    wl_surface_commit(self->cursor.surface);
}

static void _eventd_nd_cursor_frame_callback(void *data, struct wl_callback *callback, uint32_t time);

static const struct wl_callback_listener _eventd_nd_cursor_frame_wl_callback_listener = {
    .done = _eventd_nd_cursor_frame_callback,
};

static void
_eventd_nd_cursor_frame_callback(void *data, struct wl_callback *callback, uint32_t time)
{
    EventdNdBackendContext *self = data;
    int i;

    if ( self->cursor.frame_cb != NULL )
        wl_callback_destroy(self->cursor.frame_cb);
    self->cursor.frame_cb = wl_surface_frame(self->cursor.surface);
    wl_callback_add_listener(self->cursor.frame_cb, &_eventd_nd_cursor_frame_wl_callback_listener, self);

    i = wl_cursor_frame(self->cursor.cursor, time);
    _eventd_nd_cursor_set_image(self, i);
}

static void
_eventd_nd_wl_pointer_enter(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y)
{
    EventdNdWlSeat *self = data;
    EventdNdBackendContext *context = self->context;

    self->surface = wl_surface_get_user_data(surface);

    if ( context->cursor.surface == NULL )
        return;

    if ( context->cursor.cursor->image_count < 2 )
        _eventd_nd_cursor_set_image(context, 0);
    else
        _eventd_nd_cursor_frame_callback(context, context->cursor.frame_cb, 0);

    wl_pointer_set_cursor(self->pointer, serial, context->cursor.surface, context->cursor.image->hotspot_x, context->cursor.image->hotspot_y);
}

static void
_eventd_nd_wl_pointer_leave(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface)
{
    EventdNdWlSeat *self = data;
    EventdNdBackendContext *context = self->context;

    self->surface = NULL;
    if ( context->cursor.frame_cb != NULL )
        wl_callback_destroy(context->cursor.frame_cb);
}

static void
_eventd_nd_wl_pointer_motion(void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
}

static void
_eventd_nd_wl_pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t wl_button, enum wl_pointer_button_state state)
{
    EventdNdWlSeat *self = data;

    if ( self->surface == NULL )
        return;

    NkBindingsMouseButton button;
    switch ( wl_button )
    {
    case BTN_LEFT:
        button = NK_BINDINGS_MOUSE_BUTTON_PRIMARY;
    break;
    case BTN_RIGHT:
        button = NK_BINDINGS_MOUSE_BUTTON_SECONDARY;
    break;
    case BTN_MIDDLE:
        button = NK_BINDINGS_MOUSE_BUTTON_MIDDLE;
    break;
    case BTN_SIDE:
        button = NK_BINDINGS_MOUSE_BUTTON_BACK;
    break;
    case BTN_EXTRA:
        button = NK_BINDINGS_MOUSE_BUTTON_FORWARD;
    break;
    default:
        button = NK_BINDINGS_MOUSE_BUTTON_EXTRA + wl_button;
    }

    nk_bindings_seat_handle_button(self->bindings_seat, self->surface->notification, button, _eventd_nd_wl_nk_bindings_button_state[state], time);
}

static void
_eventd_nd_wl_pointer_frame(void *data, struct wl_pointer *pointer)
{
    EventdNdWlSeat *self = data;

    if ( self->surface != NULL )
    {
        NkBindingsScrollAxis axis;
        for ( axis = 0 ; axis < NK_BINDINGS_SCROLL_NUM_AXIS ; ++axis )
        {
            if ( self->scroll[axis] != 0 )
                nk_bindings_seat_handle_scroll(self->bindings_seat, self->surface->notification, axis, self->scroll[axis]);
            self->scroll[axis] = 0;
        }
    }
    NkBindingsScrollAxis axis;
    for ( axis = 0 ; axis < NK_BINDINGS_SCROLL_NUM_AXIS ; ++axis )
        self->scroll[axis] = 0;
}

static void
_eventd_nd_wl_pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time, enum wl_pointer_axis axis, wl_fixed_t value)
{
    EventdNdWlSeat *self = data;

    if ( self->surface == NULL )
        return;

    self->scroll[_eventd_nd_wl_nk_bindings_scroll_axis[axis]] += ( wl_fixed_to_int(value) / 10 );
    if ( wl_pointer_get_version(self->pointer) < WL_POINTER_FRAME_SINCE_VERSION )
        _eventd_nd_wl_pointer_frame(data, pointer);
}

static void
_eventd_nd_wl_pointer_axis_source(void *data, struct wl_pointer *pointer, enum wl_pointer_axis_source axis_source)
{
}

static void
_eventd_nd_wl_pointer_axis_stop(void *data, struct wl_pointer *pointer, uint32_t time, enum wl_pointer_axis axis)
{
}

static void
_eventd_nd_wl_pointer_axis_discrete(void *data, struct wl_pointer *pointer, enum wl_pointer_axis axis, int32_t discrete)
{
    EventdNdWlSeat *self = data;

    if ( self->surface == NULL )
        return;

    self->scroll[_eventd_nd_wl_nk_bindings_scroll_axis[axis]] += discrete;
}

static const struct wl_pointer_listener _eventd_nd_wl_pointer_listener = {
    .enter = _eventd_nd_wl_pointer_enter,
    .leave = _eventd_nd_wl_pointer_leave,
    .motion = _eventd_nd_wl_pointer_motion,
    .button = _eventd_nd_wl_pointer_button,
    .axis = _eventd_nd_wl_pointer_axis,
    .frame = _eventd_nd_wl_pointer_frame,
    .axis_source = _eventd_nd_wl_pointer_axis_source,
    .axis_stop = _eventd_nd_wl_pointer_axis_stop,
    .axis_discrete = _eventd_nd_wl_pointer_axis_discrete,
};

static void
_eventd_nd_wl_keyboard_release(EventdNdWlSeat *self)
{
    if ( self->keyboard == NULL )
        return;

    if ( wl_keyboard_get_version(self->keyboard) >= WL_KEYBOARD_RELEASE_SINCE_VERSION )
        wl_keyboard_release(self->keyboard);
    else
        wl_keyboard_destroy(self->keyboard);

    self->keyboard = NULL;

    nk_bindings_seat_update_keymap(self->bindings_seat, NULL, NULL);
}

static void
_eventd_nd_wl_pointer_release(EventdNdWlSeat *self)
{
    if ( self->pointer == NULL )
        return;

    if ( wl_pointer_get_version(self->pointer) >= WL_POINTER_RELEASE_SINCE_VERSION )
        wl_pointer_release(self->pointer);
    else
        wl_pointer_destroy(self->pointer);

    self->pointer = NULL;
}

static void
_eventd_nd_wl_seat_release(EventdNdWlSeat *self)
{
    _eventd_nd_wl_keyboard_release(self);
    _eventd_nd_wl_pointer_release(self);

    if ( wl_seat_get_version(self->seat) >= WL_SEAT_RELEASE_SINCE_VERSION )
        wl_seat_release(self->seat);
    else
        wl_seat_destroy(self->seat);

    self->context->seats = g_slist_remove_link(self->context->seats, self->link);

    nk_bindings_seat_free(self->bindings_seat);

    g_slice_free(EventdNdWlSeat, self);
}

static void
_eventd_nd_wl_seat_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities)
{
    EventdNdWlSeat *self = data;

    if ( ( capabilities & WL_SEAT_CAPABILITY_KEYBOARD ) && ( self->keyboard == NULL ) )
    {
        self->keyboard = wl_seat_get_keyboard(self->seat);
        wl_keyboard_add_listener(self->keyboard, &_eventd_nd_wl_keyboard_listener, self);
    }
    else if ( ( ! ( capabilities & WL_SEAT_CAPABILITY_KEYBOARD ) ) && ( self->keyboard != NULL ) )
        _eventd_nd_wl_keyboard_release(self);

    if ( ( capabilities & WL_SEAT_CAPABILITY_POINTER ) && ( self->pointer == NULL ) )
    {
        self->pointer = wl_seat_get_pointer(self->seat);
        wl_pointer_add_listener(self->pointer, &_eventd_nd_wl_pointer_listener, self);
    }
    else if ( ( ! ( capabilities & WL_SEAT_CAPABILITY_POINTER ) ) && ( self->pointer != NULL ) )
        _eventd_nd_wl_pointer_release(self);
}

static void
_eventd_nd_wl_seat_name(void *data, struct wl_seat *seat, const char *name)
{
}

static const struct wl_seat_listener _eventd_nd_wl_seat_listener = {
    .capabilities = _eventd_nd_wl_seat_capabilities,
    .name = _eventd_nd_wl_seat_name,
};

static const gchar * const _eventd_nd_cursor_names[] = {
    "left_ptr",
    "default",
    "top_left_arrow",
    "left-arrow",
    NULL
};

static void
_eventd_nd_wl_registry_handle_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    EventdNdBackendContext *self = data;

    if ( g_strcmp0(interface, "wl_compositor") == 0 )
    {
        self->global_names[EVENTD_ND_WL_GLOBAL_COMPOSITOR] = name;
        self->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, MIN(version, WL_COMPOSITOR_INTERFACE_VERSION));
#if CAIRO_VERSION >= CAIRO_VERSION_ENCODE(1, 14, 0)
        if ( cairo_version() >= CAIRO_VERSION_ENCODE(1, 14, 0) )
            self->scale_support = ( wl_compositor_get_version(self->compositor) >= WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION );
#endif /* CAIRO_VERION >= 1.14.0 */
    }
    else if ( g_strcmp0(interface, "zww_notification_area_v1") == 0 )
    {
        self->global_names[EVENTD_ND_WL_GLOBAL_NOTIFICATION_DAEMON] = name;
        self->notification_area = wl_registry_bind(registry, name, &zww_notification_area_v1_interface, WW_NOTIFICATION_AREA_INTERFACE_VERSION);
        zww_notification_area_v1_add_listener(self->notification_area, &_eventd_nd_wl_notification_area_listener, self);
    }
    else if ( g_strcmp0(interface, "wl_shm") == 0 )
    {
        self->global_names[EVENTD_ND_WL_GLOBAL_SHM] = name;
        self->shm = wl_registry_bind(registry, name, &wl_shm_interface, MIN(version, WL_SHM_INTERFACE_VERSION));
        wl_shm_add_listener(self->shm, &_eventd_nd_wl_shm_listener, self);
    }
    else if ( g_strcmp0(interface, "wl_seat") == 0 )
    {
        EventdNdWlSeat *seat = g_slice_new0(EventdNdWlSeat);
        seat->context = self;
        seat->global_name = name;
        seat->seat = wl_registry_bind(registry, name, &wl_seat_interface, MIN(version, WL_SEAT_INTERFACE_VERSION));
        seat->bindings_seat = nk_bindings_seat_new(self->bindings, XKB_CONTEXT_NO_FLAGS);

        seat->link = self->seats = g_slist_prepend(self->seats, seat);

        wl_seat_add_listener(seat->seat, &_eventd_nd_wl_seat_listener, seat);
    }

    if ( ( self->cursor.theme == NULL ) && ( self->compositor != NULL ) && ( self->shm != NULL ) )
    {
        self->cursor.theme = wl_cursor_theme_load(self->cursor.theme_name, 32, self->shm);
        if ( self->cursor.theme != NULL )
        {
            const gchar * const *cname = (const gchar * const *) self->cursor.name;
            for ( cname = ( cname != NULL ) ? cname : _eventd_nd_cursor_names ; ( self->cursor.cursor == NULL ) && ( *cname != NULL ) ; ++cname )
                self->cursor.cursor = wl_cursor_theme_get_cursor(self->cursor.theme, *cname);
            if ( self->cursor.cursor == NULL )
            {
                wl_cursor_theme_destroy(self->cursor.theme);
                self->cursor.theme = NULL;
            }
            else
                self->cursor.surface = wl_compositor_create_surface(self->compositor);
        }
    }
}

static void
_eventd_nd_wl_registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
    EventdNdBackendContext *self = data;

    EventdNdWlGlobalName i;
    for ( i = 0 ; i < _EVENTD_ND_WL_GLOBAL_SIZE ; ++i )
    {
        if ( self->global_names[i] != name )
            continue;
        self->global_names[i] = 0;

        switch ( i )
        {
        case EVENTD_ND_WL_GLOBAL_COMPOSITOR:
            wl_compositor_destroy(self->compositor);
            self->compositor = NULL;
        break;
        case EVENTD_ND_WL_GLOBAL_NOTIFICATION_DAEMON:
            zww_notification_area_v1_destroy(self->notification_area);
            self->notification_area = NULL;
        break;
        case EVENTD_ND_WL_GLOBAL_SHM:
            wl_shm_destroy(self->shm);
            self->shm = NULL;
        break;
        case _EVENTD_ND_WL_GLOBAL_SIZE:
            g_return_if_reached();
        }
        return;
    }
    if ( ( self->cursor.theme != NULL ) && ( ( self->compositor == NULL ) || ( self->shm == NULL ) ) )
    {
        if ( self->cursor.frame_cb != NULL )
            wl_callback_destroy(self->cursor.frame_cb);
        self->cursor.frame_cb = NULL;

        wl_surface_destroy(self->cursor.surface);
        wl_cursor_theme_destroy(self->cursor.theme);
        self->cursor.surface = NULL;
        self->cursor.image = NULL;
        self->cursor.cursor = NULL;
        self->cursor.theme = NULL;
    }

    GSList *seat_;
    for ( seat_ = self->seats ; seat_ != NULL ; seat_ = g_slist_next(seat_) )
    {
        EventdNdWlSeat *seat = seat_->data;
        if ( seat->global_name != name )
            continue;

        _eventd_nd_wl_seat_release(seat);
        return;
    }
}

static const struct wl_registry_listener _eventd_nd_wl_registry_listener = {
    .global = _eventd_nd_wl_registry_handle_global,
    .global_remove = _eventd_nd_wl_registry_handle_global_remove,
};

static gboolean
_eventd_nd_wayland_display_remove_callback(gpointer user_data)
{
    EventdNdBackendContext *self = user_data;

    if ( errno > 0 )
        g_warning("Error in Wayland connection: %s", g_strerror(errno));
    else
        g_warning("Error in Wayland connection");

    self->nd->backend_stop(self->nd->context);

    return FALSE;
}

static void _eventd_nd_wl_stop(EventdNdBackendContext *self);
static gboolean
_eventd_nd_wl_start(EventdNdBackendContext *self, const gchar *target)
{
    self->source = g_water_wayland_source_new(NULL, target);
    if ( self->source == NULL )
        return FALSE;
    g_water_wayland_source_set_error_callback(self->source, _eventd_nd_wayland_display_remove_callback, self, NULL);

    self->display = g_water_wayland_source_get_display(self->source);
    self->registry = wl_display_get_registry(self->display);
    wl_registry_add_listener(self->registry, &_eventd_nd_wl_registry_listener, self);
    wl_display_roundtrip(self->display);

    if ( self->notification_area == NULL )
    {
        _eventd_nd_wl_stop(self);
        g_warning("No ww_notification_daemon interface provided by the compositor");
        return FALSE;
    }

    self->nd->shaping_update(self->nd->context, EVENTD_ND_SHAPING_COMPOSITING);

    return TRUE;
}

static void
_eventd_nd_wl_stop(EventdNdBackendContext *self)
{
    GSList *seat_ = self->seats;
    while ( seat_ != NULL )
    {
        EventdNdWlSeat *seat = seat_->data;
        GSList *next_ = g_slist_next(seat_);
        _eventd_nd_wl_seat_release(seat);
        seat_ = next_;
    }

    EventdNdWlGlobalName i;
    for ( i = 0 ; i < _EVENTD_ND_WL_GLOBAL_SIZE ; ++i )
    {
        if ( self->global_names[i] != 0 )
            _eventd_nd_wl_registry_handle_global_remove(self, self->registry, self->global_names[i]);
    }

    wl_registry_destroy(self->registry);
    self->registry = NULL;

    g_water_wayland_source_free(self->source);
    self->display = NULL;
    self->source = NULL;
}

static void
_eventd_nd_wl_buffer_release(gpointer data, struct wl_buffer *buf)
{
    EventdNdWlBuffer *self = data;

    if ( self->released )
    {
        wl_buffer_destroy(self->buffer);
        munmap(self->data, self->size);
        g_free(self);
    }
    else
        self->released = TRUE;
}

static const struct wl_buffer_listener _eventd_nd_wl_buffer_listener = {
    _eventd_nd_wl_buffer_release
};

static gboolean
_eventd_nd_wl_create_buffer(EventdNdSurface *self)
{
    struct wl_shm_pool *pool;
    struct wl_buffer *buffer;
    gint fd;
    gpointer data;
    gint width, height, stride;
    gsize size;

    width = self->width * self->context->scale;
    height = self->height * self->context->scale;
    stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
    size = stride * height;

    fd = shm_open("/eventd-nd-wayland-surface", O_CREAT | O_RDWR, 0);
    shm_unlink("/eventd-nd-wayland-surface");
    if ( fd < 0 )
    {
        g_warning("creating a buffer file for %zu B failed: %s\n", size, g_strerror(errno));
        return FALSE;
    }
    if ( ftruncate(fd, size) < 0 )
    {
        g_close(fd, NULL);
        return FALSE;
    }

    data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if ( data == MAP_FAILED )
    {
        g_warning("mmap failed: %s\n", g_strerror(errno));
        g_close(fd, NULL);
        return FALSE;
    }

    cairo_surface_t *cairo_surface;
    cairo_surface = cairo_image_surface_create_for_data(data, CAIRO_FORMAT_ARGB32, width, height, 4 * width);
#if CAIRO_VERSION >= CAIRO_VERSION_ENCODE(1, 14, 0)
    if ( self->context->scale_support )
        cairo_surface_set_device_scale(cairo_surface, self->context->scale, self->context->scale);
#endif /* CAIRO_VERION >= 1.14.0 */
    self->context->nd->notification_draw(self->notification, cairo_surface);
    cairo_surface_destroy(cairo_surface);

    munmap(data, size);

    pool = wl_shm_create_pool(self->context->shm, fd, size);
    buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    g_close(fd, NULL);

    if ( self->buffer != NULL )
        _eventd_nd_wl_buffer_release(self->buffer, self->buffer->buffer);

    self->buffer = g_new0(EventdNdWlBuffer, 1);
    self->buffer->buffer = buffer;
    self->buffer->data = data;
    self->buffer->size = size;

    wl_buffer_add_listener(buffer, &_eventd_nd_wl_buffer_listener, self->buffer);

    wl_surface_damage(self->surface, 0, 0, self->width, self->height);
    wl_surface_attach(self->surface, self->buffer->buffer, 0, 0);
#if CAIRO_VERSION >= CAIRO_VERSION_ENCODE(1, 14, 0)
    if ( self->context->scale_support )
        wl_surface_set_buffer_scale(self->surface, self->context->scale);
#endif /* CAIRO_VERION >= 1.14.0 */
    wl_surface_commit(self->surface);

    return TRUE;
}

static EventdNdSurface *
_eventd_nd_wl_surface_new(EventdNdBackendContext *context, EventdNdNotification *notification, gint width, gint height)
{
    EventdNdSurface *self;

    self = g_new0(EventdNdSurface, 1);
    self->context = context;
    self->notification = notification;
    self->width = width;
    self->height = height;

    self->surface = wl_compositor_create_surface(context->compositor);
    wl_surface_set_user_data(self->surface, self);

    if ( ! _eventd_nd_wl_create_buffer(self) )
    {
        wl_surface_destroy(self->surface);
        g_free(self);
        return NULL;
    }

    self->ww_notification = zww_notification_area_v1_create_notification(context->notification_area, self->surface);

    return self;
}

static void
_eventd_nd_wl_surface_update(EventdNdSurface *self, gint width, gint height)
{
    self->width = width;
    self->height = height;
    _eventd_nd_wl_create_buffer(self);
}

static void
_eventd_nd_wl_surface_free(EventdNdSurface *self)
{
    if ( self == NULL )
        return;

    _eventd_nd_wl_buffer_release(self->buffer, self->buffer->buffer);

    zww_notification_v1_destroy(self->ww_notification);
    wl_surface_destroy(self->surface);

    g_free(self);
}

static void
_eventd_nd_wl_move_surface(EventdNdSurface *self, gint x, gint y, gpointer data)
{
    zww_notification_v1_move(self->ww_notification, x, y);
    if ( self->context->need_redraw )
        _eventd_nd_wl_create_buffer(self);
}

static void
_eventd_nd_wl_move_end(EventdNdBackendContext *self, gpointer data)
{
    self->need_redraw = FALSE;
}

EVENTD_EXPORT const gchar *eventd_nd_backend_id = "eventd-nd-wayland";
EVENTD_EXPORT
void
eventd_nd_backend_get_info(EventdNdBackend *backend)
{
    backend->init   = _eventd_nd_wl_init;
    backend->uninit = _eventd_nd_wl_uninit;

    backend->global_parse = _eventd_nd_wl_global_parse;
    backend->config_reset = _eventd_nd_wl_config_reset;

    backend->start = _eventd_nd_wl_start;
    backend->stop  = _eventd_nd_wl_stop;

    backend->surface_new    = _eventd_nd_wl_surface_new;
    backend->surface_update = _eventd_nd_wl_surface_update;
    backend->surface_free   = _eventd_nd_wl_surface_free;

    backend->move_surface = _eventd_nd_wl_move_surface;
    backend->move_end = _eventd_nd_wl_move_end;
}
