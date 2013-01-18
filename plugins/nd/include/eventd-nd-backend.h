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

#ifndef __EVENTD_ND_BACKEND_H__
#define __EVENTD_ND_BACKEND_H__

#include <libeventd-event-types.h>

typedef struct _EventdPluginContext EventdNdContext;
typedef struct _EventdNdDisplay EventdNdDisplay;
typedef struct _EventdNdSurface EventdNdSurface;
typedef struct _EventdNdBackendContext EventdNdBackendContext;

typedef void (*EventdNdRemoveDisplayFunc)(EventdNdContext *context, EventdNdDisplay *display);

typedef struct {
    EventdNdRemoveDisplayFunc remove_display;
} EventdNdInterface;

typedef EventdNdBackendContext *(*EventdNdBackendInitFunc)(EventdNdContext *nd, EventdNdInterface *nd_interface);
typedef void (*EventdNdBackendUninitFunc)(EventdNdBackendContext *context);

typedef void (*EventdNdBackendGlobalParseFunc)(EventdNdBackendContext *context, GKeyFile *config_file);

typedef EventdNdDisplay *(*EventdNdDisplayNewFunc)(EventdNdBackendContext *context, const gchar *target);
typedef void (*EventdNdDisplayFunc)(EventdNdDisplay *display);

typedef EventdNdSurface *(*EventdNdSurfaceNewFunc)(EventdEvent *event, EventdNdDisplay *display, cairo_surface_t *bubble);
typedef void (*EventdNdSurfaceFunc)(EventdNdSurface *surface);
typedef void(*EventdNdSurfaceUpdateFunc)(EventdNdSurface *surface, cairo_surface_t *bubble);
typedef void (*EventdNdSurfaceDisplayFunc)(EventdNdSurface *surface, gint x, gint y);

typedef struct {
    EventdNdBackendInitFunc init;
    EventdNdBackendUninitFunc uninit;

    EventdNdBackendGlobalParseFunc global_parse;

    EventdNdDisplayNewFunc display_new;
    EventdNdDisplayFunc display_free;

    EventdNdSurfaceNewFunc surface_new;
    EventdNdSurfaceFunc surface_free;
    EventdNdSurfaceUpdateFunc surface_update;
    EventdNdSurfaceDisplayFunc surface_display;

    /* private */
    gpointer module;
    EventdNdBackendContext *context;
} EventdNdBackend;

typedef void (*EventdNdBackendGetInfoFunc)(EventdNdBackend *backend);

#endif /* __EVENTD_ND_BACKEND_H__ */
