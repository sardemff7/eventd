/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011 Quentin "Sardem FF7" Glidic
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

#if DISABLE_FRAMEBUFFER_BACKENDS
#include "graphical.h"

EventdNdSurface *eventd_nd_graphical_surface_new(EventdNdDisplay *display, gint margin, EventdNdStyleAnchor anchor, gint width, gint height, cairo_surface_t *bubble, cairo_surface_t *shape) { return NULL; }
void eventd_nd_graphical_surface_show(EventdNdSurface *surface) {}
void eventd_nd_graphical_surface_hide(EventdNdSurface *surface) {}
void eventd_nd_graphical_surface_free(gpointer data) {}

#endif /* DISABLE_FRAMEBUFFER_BACKENDS */

#if DISABLE_FRAMEBUFFER_BACKENDS
#include "fb.h"

EventdNdSurface *eventd_nd_fb_surface_new(EventdNdDisplay *display, gint margin, EventdNdStyleAnchor anchor, gint width, gint height, cairo_surface_t *bubble, cairo_surface_t *shape) { return NULL; }
void eventd_nd_fb_surface_show(EventdNdSurface *surface) {}
void eventd_nd_fb_surface_hide(EventdNdSurface *surface) {}
void eventd_nd_fb_surface_free(gpointer data) {}

#endif /* DISABLE_FRAMEBUFFER_BACKENDS */

