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

#ifndef __EVENTD_PLUGINS_NOTIFICATION_DAEMON_GRAPHICAL_BACKEND_H__
#define __EVENTD_PLUGINS_NOTIFICATION_DAEMON_GRAPHICAL_BACKEND_H__

EventdNdDisplay *eventd_nd_graphical_display_new(const gchar *target, EventdNdStyleAnchor anchor, gint margin);
void eventd_nd_graphical_display_free(gpointer data);

EventdNdSurface *eventd_nd_graphical_surface_new(EventdNdDisplay *display, gint width, gint height, cairo_surface_t *bubble, cairo_surface_t *shape);
void eventd_nd_graphical_surface_show(EventdNdSurface *surface);
void eventd_nd_graphical_surface_hide(EventdNdSurface *surface);
void eventd_nd_graphical_surface_free(gpointer data);

#endif /* __EVENTD_PLUGINS_NOTIFICATION_DAEMON_GRAPHICAL_BACKEND_H__ */
