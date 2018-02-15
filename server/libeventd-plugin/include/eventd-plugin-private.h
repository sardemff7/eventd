/*
 * libeventd-plugin - Library to implement an eventd plugin
 *
 * Copyright © 2011-2018 Quentin "Sardem FF7" Glidic
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with eventd. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __EVENTD_EVENTD_PLUGIN_PRIVATE_H__
#define __EVENTD_EVENTD_PLUGIN_PRIVATE_H__

#include "eventd-plugin.h"

typedef GList *(*EventdPluginCoreGetSocketsFunc)(EventdPluginCoreContext *context, GSocketAddress **binds);

typedef gboolean (*EventdPluginCorePushEventFunc)(EventdPluginCoreContext *context, EventdEvent *event);

typedef struct {
    EventdPluginCoreGetSocketsFunc get_sockets;

    EventdPluginCorePushEventFunc push_event;
} EventdCoreInterface;

struct EventdPluginInterface {
    EventdPluginInitFunc init;
    EventdPluginSimpleFunc uninit;

    EventdPluginSimpleFunc start;
    EventdPluginSimpleFunc stop;

    EventdPluginControlCommandFunc control_command;

    EventdPluginGlobalParseFunc global_parse;
    EventdPluginActionParseFunc action_parse;
    EventdPluginSimpleFunc config_reset;

    EventdPluginEventDispatchFunc event_dispatch;
    EventdPluginEventActionFunc event_action;
};

#endif /* __EVENTD_EVENTD_PLUGIN_PRIVATE_H__ */
