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

typedef enum {
    EVENTD_ND_STYLE_ANCHOR_TOP_LEFT,
    EVENTD_ND_STYLE_ANCHOR_TOP_RIGHT,
    EVENTD_ND_STYLE_ANCHOR_BOTTOM_LEFT,
    EVENTD_ND_STYLE_ANCHOR_BOTTOM_RIGHT
} EventdNdStyleAnchor;

typedef struct _EventdNdDisplay EventdNdDisplay;
typedef struct _EventdNdSurface EventdNdSurface;
typedef struct _EventdNdBackendContext EventdNdBackendContext;

typedef EventdNdBackendContext *(*EventdNdBackendInitFunc)();
typedef void (*EventdNdBackendUninitFunc)(EventdNdBackendContext *context);

typedef gboolean (*EventdNdDisplayTestFunc)(EventdNdBackendContext *context, const gchar *target);
typedef EventdNdDisplay *(*EventdNdDisplayNewFunc)(EventdNdBackendContext *context, const gchar *target, EventdNdStyleAnchor anchor, gint margin);
typedef void (*EventdNdDisplayFunc)(EventdNdDisplay *display);

typedef EventdNdSurface *(*EventdNdSurfaceShowFunc)(EventdEvent *event, EventdNdDisplay *display, EventdNdNotification *notification, EventdNdStyle *style);
typedef void (*EventdNdSurfaceHideFunc)(EventdNdSurface *surface);

typedef struct {
    EventdNdBackendInitFunc init;
    EventdNdBackendUninitFunc uninit;

    EventdNdDisplayTestFunc display_test;
    EventdNdDisplayNewFunc display_new;
    EventdNdDisplayFunc display_free;

    EventdNdSurfaceShowFunc surface_show;
    EventdNdSurfaceHideFunc surface_hide;

    /* private */
    gpointer module;
    EventdNdBackendContext *context;
} EventdNdBackend;

typedef void (*EventdNdBackendGetInfoFunc)(EventdNdBackend *backend);

#endif /* __EVENTD_ND_BACKEND_H__ */
