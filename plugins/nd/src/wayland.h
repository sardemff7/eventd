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

#include "nkutils-bindings.h"

#include "notification-manager-unstable-v1-client-protocol.h"

#include "types.h"

EventdNdQueue *eventd_nd_queue_new(EventdNdContext *context, const gchar *name, struct zeventd_nw_notification_queue_v1 *nw_queue);

void eventd_nd_notification_draw(EventdNdNotification *notification, cairo_surface_t *surface);


typedef struct _EventdNdWayland EventdNdWayland;
typedef struct _EventdNdSurface EventdNdSurface;

EventdNdWayland *eventd_nd_wl_init(EventdNdContext *context, NkBindings *bindings);
void eventd_nd_wl_uninit(EventdNdWayland *context);

void eventd_nd_wl_global_parse(EventdNdWayland *context, GKeyFile *config_file);
void eventd_nd_wl_config_reset(EventdNdWayland *context);

gboolean eventd_nd_wl_start(EventdNdWayland *context, const gchar *target);
void eventd_nd_wl_stop(EventdNdWayland *context);

EventdNdSurface *eventd_nd_wl_surface_new(EventdNdWayland *context, EventdNdNotification *notification, EventdNdQueue *queue);
void eventd_nd_wl_surface_update(EventdNdSurface *surface, EventdNdSize size, EventdNdGeometry geometry);
void eventd_nd_wl_surface_free(EventdNdSurface *surface);

#endif /* __EVENTD_ND_WAYLAND_H__ */
