/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2021 Quentin "Sardem FF7" Glidic
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

#ifndef __EVENTD_ND_ND_H__
#define __EVENTD_ND_ND_H__

#include "types.h"
#include <nkutils-xdg-theme.h>

struct _EventdNdQueue {
    EventdNdAnchor anchor;
    guint64 limit;
    gint margin_x;
    gint margin_y;
    gint spacing;
    gboolean reverse;
    gboolean more_indicator;
    EventdEvent *more_event;
    EventdNdNotification *more_notification;
    GQueue *wait_queue;
    GQueue *queue;
};

struct _EventdPluginContext {
    EventdPluginCoreContext *core;
    EventdNdInterface interface;
    NkBindings *bindings;
    EventdNdBackend backends[_EVENTD_ND_BACKENDS_SIZE];
    EventdNdBackend *backend;
    GHashTable *queues;
    EventdNdStyle *style;
    EventdNdBackends last_backend;
    NkXdgThemeContext *theme_context;
    gchar *last_target;
    struct {
        gint x;
        gint y;
        gint w;
        gint h;
        gint s;
    } geometry;
    GHashTable *notifications;
    gboolean no_refresh;
    EventdNdShaping shaping;
    GSList *actions;
};

#endif /* __EVENTD_ND_ND_H__ */
