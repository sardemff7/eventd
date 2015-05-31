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

typedef enum {
	EVENTD_PLUGIN_COMMAND_STATUS_OK            = 0,
	EVENTD_PLUGIN_COMMAND_STATUS_COMMAND_ERROR = 30,
	EVENTD_PLUGIN_COMMAND_STATUS_EXEC_ERROR    = 31,
} EventdPluginCommandStatus;


/*
 * eventd plugin interface
 */

typedef EventdPluginContext *(*EventdPluginInitFunc)(EventdCoreContext *core, EventdCoreInterface *interface);
typedef void (*EventdPluginSimpleFunc)(EventdPluginContext *context);
typedef GOptionGroup *(*EventdPluginGetOptionGroupFunc)(EventdPluginContext *context);
typedef EventdPluginCommandStatus (*EventdPluginControlCommandFunc)(EventdPluginContext *context, guint64 argc, const gchar * const *argv, gchar **status);
typedef void (*EventdPluginGlobalParseFunc)(EventdPluginContext *context, GKeyFile *);
typedef void (*EventdPluginEventParseFunc)(EventdPluginContext *context, const gchar *config_id, GKeyFile *key_file);
typedef void (*EventdPluginEventDispatchFunc)(EventdPluginContext *context, const gchar *config_id, EventdEvent *event);

typedef void (*EventdPluginGetInterfaceFunc)(EventdPluginInterface *interface);

void libeventd_plugin_interface_add_init_callback(EventdPluginInterface *interface, EventdPluginInitFunc callback);
void libeventd_plugin_interface_add_uninit_callback(EventdPluginInterface *interface, EventdPluginSimpleFunc callback);

void libeventd_plugin_interface_add_get_option_group_callback(EventdPluginInterface *interface, EventdPluginGetOptionGroupFunc callback);

void libeventd_plugin_interface_add_start_callback(EventdPluginInterface *interface, EventdPluginSimpleFunc callback);
void libeventd_plugin_interface_add_stop_callback(EventdPluginInterface *interface, EventdPluginSimpleFunc callback);

void libeventd_plugin_interface_add_control_command_callback(EventdPluginInterface *interface, EventdPluginControlCommandFunc callback);

void libeventd_plugin_interface_add_config_init_callback(EventdPluginInterface *interface, EventdPluginSimpleFunc callback);
void libeventd_plugin_interface_add_global_parse_callback(EventdPluginInterface *interface, EventdPluginGlobalParseFunc callback);
void libeventd_plugin_interface_add_event_parse_callback(EventdPluginInterface *interface, EventdPluginEventParseFunc callback);
void libeventd_plugin_interface_add_config_reset_callback(EventdPluginInterface *interface, EventdPluginSimpleFunc callback);

void libeventd_plugin_interface_add_event_action_callback(EventdPluginInterface *interface, EventdPluginEventDispatchFunc callback);


/*
 * eventd core interface
 */

GList *libeventd_core_get_sockets(EventdCoreContext *context, EventdCoreInterface *interface, const gchar * const *binds);

gboolean libeventd_core_push_event(EventdCoreContext *context, EventdCoreInterface *interface, EventdEvent *event);

#endif /* __EVENTD_EVENTD_PLUGIN_H__ */
