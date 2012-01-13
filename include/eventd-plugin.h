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

#ifndef __EVENTD_EVENTD_PLUGIN_H__
#define __EVENTD_EVENTD_PLUGIN_H__

#include <libeventd-event-types.h>

typedef struct _EventdPluginContext EventdPluginContext;

typedef EventdPluginContext *(*EventdPluginStartFunc)(gpointer user_data);
typedef void (*EventdPluginStopFunc)(EventdPluginContext *context);
typedef void (*EventdPluginControlCommandFunc)(EventdPluginContext *context, const gchar *command);
typedef void (*EventdPluginConfigInitFunc)(EventdPluginContext *context);
typedef void (*EventdPluginConfigCleanFunc)(EventdPluginContext *context);
typedef void (*EventdPluginEventParseFunc)(EventdPluginContext *context, const gchar *, const gchar *, GKeyFile *);
typedef void (*EventdPluginEventDispatchFunc)(EventdPluginContext *context, EventdEvent *event);

typedef struct {
    EventdPluginStartFunc start;
    EventdPluginStopFunc stop;

    EventdPluginControlCommandFunc control_command;

    EventdPluginConfigInitFunc config_init;
    EventdPluginConfigCleanFunc config_clean;

    EventdPluginEventParseFunc event_parse;
    EventdPluginEventDispatchFunc event_action;
    EventdPluginEventDispatchFunc event_pong;

    /* Private stuff */
    void *module;
    EventdPluginContext *context;
} EventdPlugin;

typedef EventdPlugin *(*EventdPluginGetInfoFunc)(EventdPlugin *plugin);

#endif /* __EVENTD_EVENTD_PLUGIN_H__ */
