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

#ifndef __EVENTD_ND_WAYLAND_H__
#define __EVENTD_ND_WAYLAND_H__

#include "libeventd-event.h"
#include "eventd-plugin.h"

#include "types.h"

void eventd_nd_shaping_update(EventdNdContext *context, EventdNdShaping shaping);
void eventd_nd_geometry_update(EventdNdContext *context, gint w, gint h, gint scale);

void eventd_nd_notification_shape(EventdNdNotification *notification, cairo_t *cr);
void eventd_nd_notification_draw(EventdNdNotification *notification, cairo_surface_t *surface);


typedef struct _EventdNdBackendContext EventdNdBackendContext;
typedef struct _EventdNdSurface EventdNdSurface;

EventdNdBackendContext *eventd_nd_wl_init(EventdNdContext *context, NkBindings *bindings);
void eventd_nd_wl_uninit(EventdNdBackendContext *context);

void eventd_nd_wl_global_parse(EventdNdBackendContext *context, GKeyFile *config_file);
void eventd_nd_wl_config_reset(EventdNdBackendContext *context);

gboolean eventd_nd_wl_start(EventdNdBackendContext *context, const gchar *target);
void eventd_nd_wl_stop(EventdNdBackendContext *context);

EventdNdSurface *eventd_nd_wl_surface_new(EventdNdBackendContext *context, EventdNdNotification *notification, gint width, gint height);
void eventd_nd_wl_surface_update(EventdNdSurface *surface, gint width, gint height);
void eventd_nd_wl_surface_free(EventdNdSurface *surface);

void eventd_nd_wl_move_surface(EventdNdSurface *surface, gint x, gint y);
void eventd_nd_wl_move_end(EventdNdBackendContext *context);

#endif /* __EVENTD_ND_WAYLAND_H__ */
