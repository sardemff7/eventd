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
#include "types.h"
#include <nkutils-xdg-theme.h>

struct _EventdNdQueue {
    EventdPluginContext *context;
    gchar *name;
    struct zeventd_nw_notification_queue_v1 *nw_queue;
    struct {
        gint w;
        gint h;
        gint s;
    } geometry;
    GQueue *queue;
};

struct _EventdPluginContext {
    EventdPluginCoreContext *core;
    NkBindings *bindings;
    EventdNdWayland *wayland;
    GHashTable *queues;
    EventdNdStyle *style;
    NkXdgThemeContext *theme_context;
    gchar *last_target;
    GHashTable *notifications;
    gboolean no_refresh;
    GSList *actions;
};

#endif /* __EVENTD_ND_ND_H__ */
