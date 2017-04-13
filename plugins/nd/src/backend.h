/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2017 Quentin "Sardem FF7" Glidic
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

#include "libeventd-event.h"

#include "types.h"

typedef enum {
    EVENTD_ND_BACKEND_NONE = 0,
    EVENTD_ND_BACKEND_WAYLAND,
    EVENTD_ND_BACKEND_XCB,
    EVENTD_ND_BACKEND_FBDEV,
    EVENTD_ND_BACKEND_WIN,
    _EVENTD_ND_BACKENDS_SIZE
} EventdNdBackends;

const gchar *eventd_nd_backends_names[_EVENTD_ND_BACKENDS_SIZE];

typedef struct {
    EventdNdContext *context;

    void (*geometry_update)(EventdNdContext *context, gint w, gint h, gint scale);
    gboolean (*backend_stop)(EventdNdContext *context);

    void (*notification_shape)(EventdNdNotification *notification, cairo_t *cr);
    void (*notification_draw)(EventdNdNotification *notification, cairo_surface_t *surface, gboolean shaped);
    void (*notification_dismiss)(EventdNdNotification *notification);
} EventdNdInterface;


typedef struct _EventdNdBackendContext EventdNdBackendContext;
typedef struct _EventdNdSurface EventdNdSurface;

typedef struct {
    EventdNdBackendContext *(*init)(EventdNdInterface *context);
    void (*uninit)(EventdNdBackendContext *context);

    void (*global_parse)(EventdNdBackendContext *context, GKeyFile *config_file);
    void (*config_reset)(EventdNdBackendContext *context);

    gboolean (*start)(EventdNdBackendContext *context, const gchar *target);
    void (*stop)(EventdNdBackendContext *context);

    EventdNdSurface *(*surface_new)(EventdNdBackendContext *context, EventdNdNotification *notification, gint width, gint height);
    void (*surface_update)(EventdNdSurface *surface, gint width, gint height);
    void (*surface_free)(EventdNdSurface *surface);

    gpointer (*move_begin)(EventdNdBackendContext *context, gsize count);
    void (*move_surface)(EventdNdSurface *surface, gint x, gint y, gpointer data);
    void (*move_end)(EventdNdBackendContext *context, gpointer data);

    gpointer module;
    EventdNdBackendContext *context;
} EventdNdBackend;

typedef void (*EventdNdBackendGetInfoFunc)(EventdNdBackend *backend);


static inline gint
_eventd_nd_compute_scale(gint w, gint h, gint mm_w, gint mm_h)
{
    gdouble dpi_x = ( (gdouble) w * 25.4 ) / (gdouble) mm_w;
    gdouble dpi_y = ( (gdouble) h * 25.4 ) / (gdouble) mm_h;
    gdouble dpi = MIN(dpi_x, dpi_y);

    return (gint) ( dpi / 96. + 0.25 );
}

#endif /* __EVENTD_ND_BACKEND_H__ */
