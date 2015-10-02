/*
 * libeventd-plugin - Library to implement an eventd plugin
 *
 * Copyright © 2011-2015 Quentin "Sardem FF7" Glidic
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

#ifndef __EVENTD_EVENTD_PLUGIN_H__
#define __EVENTD_EVENTD_PLUGIN_H__

#include <libeventd-event.h>

G_BEGIN_DECLS

typedef struct _EventdCoreContext EventdPluginCoreContext;
typedef struct _EventdPluginCoreInterface EventdPluginCoreInterface;

typedef struct _EventdPluginContext EventdPluginContext;
typedef struct EventdPluginInterface EventdPluginInterface;

typedef enum {
	EVENTD_PLUGIN_COMMAND_STATUS_OK            = 0,
	EVENTD_PLUGIN_COMMAND_STATUS_COMMAND_ERROR = 30,
	EVENTD_PLUGIN_COMMAND_STATUS_EXEC_ERROR    = 31,
} EventdPluginCommandStatus;

GType eventd_plugin_command_status_get_type(void) G_GNUC_CONST;
#define EVENTD_PLUGIN_TYPE_COMMAND_STATUS (eventd_plugin_command_status_get_type())

/*
 * eventd plugin interface
 */

typedef EventdPluginContext *(*EventdPluginInitFunc)(EventdPluginCoreContext *core, EventdPluginCoreInterface *interface);
typedef void (*EventdPluginSimpleFunc)(EventdPluginContext *context);
typedef GOptionGroup *(*EventdPluginGetOptionGroupFunc)(EventdPluginContext *context);
typedef EventdPluginCommandStatus (*EventdPluginControlCommandFunc)(EventdPluginContext *context, guint64 argc, const gchar * const *argv, gchar **status);
typedef void (*EventdPluginGlobalParseFunc)(EventdPluginContext *context, GKeyFile *key_file);
typedef void (*EventdPluginEventParseFunc)(EventdPluginContext *context, const gchar *config_id, GKeyFile *key_file);
typedef void (*EventdPluginEventDispatchFunc)(EventdPluginContext *context, const gchar *config_id, EventdEvent *event);

typedef void (*EventdPluginGetInterfaceFunc)(EventdPluginInterface *interface);

void eventd_plugin_interface_add_init_callback(EventdPluginInterface *interface, EventdPluginInitFunc callback);
void eventd_plugin_interface_add_uninit_callback(EventdPluginInterface *interface, EventdPluginSimpleFunc callback);

void eventd_plugin_interface_add_get_option_group_callback(EventdPluginInterface *interface, EventdPluginGetOptionGroupFunc callback);

void eventd_plugin_interface_add_start_callback(EventdPluginInterface *interface, EventdPluginSimpleFunc callback);
void eventd_plugin_interface_add_stop_callback(EventdPluginInterface *interface, EventdPluginSimpleFunc callback);

void eventd_plugin_interface_add_control_command_callback(EventdPluginInterface *interface, EventdPluginControlCommandFunc callback);

void eventd_plugin_interface_add_global_parse_callback(EventdPluginInterface *interface, EventdPluginGlobalParseFunc callback);
void eventd_plugin_interface_add_event_parse_callback(EventdPluginInterface *interface, EventdPluginEventParseFunc callback);
void eventd_plugin_interface_add_config_reset_callback(EventdPluginInterface *interface, EventdPluginSimpleFunc callback);

void eventd_plugin_interface_add_event_action_callback(EventdPluginInterface *interface, EventdPluginEventDispatchFunc callback);


/*
 * eventd core interface
 */

GList *eventd_plugin_core_get_sockets(EventdPluginCoreContext *context, EventdPluginCoreInterface *interface, const gchar * const *binds);

gboolean eventd_plugin_core_push_event(EventdPluginCoreContext *context, EventdPluginCoreInterface *interface, EventdEvent *event);

G_END_DECLS

#endif /* __EVENTD_EVENTD_PLUGIN_H__ */
