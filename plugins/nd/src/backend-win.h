/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2015 Quentin "Sardem FF7" Glidic
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

#ifndef __EVENTD_ND_BACKEND_WIN_H__
#define __EVENTD_ND_BACKEND_WIN_H__

#include "backend.h"

EventdNdBackendContext *eventd_nd_win_init(EventdNdContext *nd);
void eventd_nd_win_uninit(EventdNdBackendContext *context);

void eventd_nd_win_global_parse(EventdNdBackendContext *context, GKeyFile *config_file);

gboolean eventd_nd_win_start(EventdNdBackendContext *context, const gchar *target);
void eventd_nd_win_stop(EventdNdBackendContext *context);

EventdNdSurface *eventd_nd_win_surface_new(EventdNdBackendContext *context, EventdEvent *event, cairo_surface_t *bubble);
void eventd_nd_win_surface_update(EventdNdSurface *surface, cairo_surface_t *bubble);
void eventd_nd_win_surface_free(EventdNdSurface *surface);

#endif /* __EVENTD_ND_BACKEND_WIN_H__ */
