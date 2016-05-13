/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2016 Quentin "Sardem FF7" Glidic
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

#ifndef __EVENTD_ND_NOTIFICATION_H__
#define __EVENTD_ND_NOTIFICATION_H__

#include "types.h"

void eventd_nd_notification_geometry_changed(EventdPluginContext *context, gboolean resize);
void eventd_nd_notification_dismiss_target(EventdPluginContext *context, EventdNdDismissTarget target, EventdNdAnchor anchor);

EventdNdNotification *eventd_nd_notification_new(EventdPluginContext *context, EventdEvent *event, EventdNdStyle *style);
void eventd_nd_notification_free(gpointer data);

void eventd_nd_notification_shape(EventdNdNotification *notification, cairo_t *cr);
void eventd_nd_notification_draw(EventdNdNotification *notification, cairo_surface_t *surface, gboolean shaped);
void eventd_nd_notification_update(EventdNdNotification *notification, EventdEvent *event);
void eventd_nd_notification_dismiss(EventdNdNotification *notification);

#endif /* __EVENTD_ND_NOTIFICATION_H__ */
