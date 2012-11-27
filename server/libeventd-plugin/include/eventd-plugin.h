/*
 * libeventd-plugin - Library to implement an eventd plugin
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

typedef struct _EventdCoreContext EventdCoreContext;
typedef struct _EventdCoreInterface EventdCoreInterface;

typedef struct _EventdPluginContext EventdPluginContext;
typedef struct EventdPluginInterface EventdPluginInterface;


/*
 * eventd plugin interface
 */

typedef EventdPluginContext *(*EventdPluginInitFunc)(EventdCoreContext *core, EventdCoreInterface *interface);
typedef void (*EventdPluginSimpleFunc)(EventdPluginContext *context);
typedef GOptionGroup *(*EventdPluginGetOptionGroupFunc)(EventdPluginContext *context);
typedef void (*EventdPluginControlCommandFunc)(EventdPluginContext *context, const gchar *command);
typedef void (*EventdPluginGlobalParseFunc)(EventdPluginContext *context, GKeyFile *);
typedef void (*EventdPluginEventParseFunc)(EventdPluginContext *context, const gchar *config_id, GKeyFile *key_file);
typedef void (*EventdPluginEventDispatchFunc)(EventdPluginContext *context, const gchar *config_id, EventdEvent *event);

typedef void (*EventdPluginGetInterfaceFunc)(EventdPluginInterface *interface);

#define LIBEVENTD_PLUGIN_INTERFACE_ADD_CALLBACK(action, Action) void libeventd_plugin_interface_add_##action##_callback(EventdPluginInterface *interface, EventdPlugin##Action##Func callback)


LIBEVENTD_PLUGIN_INTERFACE_ADD_CALLBACK(init, Init);
LIBEVENTD_PLUGIN_INTERFACE_ADD_CALLBACK(uninit, Simple);

LIBEVENTD_PLUGIN_INTERFACE_ADD_CALLBACK(get_option_group, GetOptionGroup);

LIBEVENTD_PLUGIN_INTERFACE_ADD_CALLBACK(start, Simple);
LIBEVENTD_PLUGIN_INTERFACE_ADD_CALLBACK(stop, Simple);

LIBEVENTD_PLUGIN_INTERFACE_ADD_CALLBACK(control_command, ControlCommand);

LIBEVENTD_PLUGIN_INTERFACE_ADD_CALLBACK(global_parse, GlobalParse);
LIBEVENTD_PLUGIN_INTERFACE_ADD_CALLBACK(event_parse, EventParse);
LIBEVENTD_PLUGIN_INTERFACE_ADD_CALLBACK(config_reset, Simple);

LIBEVENTD_PLUGIN_INTERFACE_ADD_CALLBACK(event_action, EventDispatch);


/*
 * eventd core interface
 */

GList *libeventd_core_get_sockets(EventdCoreContext *context, EventdCoreInterface *interface, const gchar * const *binds);

const gchar *libeventd_core_get_event_config_id(EventdCoreContext *context, EventdCoreInterface *interface, EventdEvent *event);
void libeventd_core_push_event(EventdCoreContext *context, EventdCoreInterface *interface, const gchar *config_id, EventdEvent *event);

#endif /* __EVENTD_EVENTD_PLUGIN_H__ */
