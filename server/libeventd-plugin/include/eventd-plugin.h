/*
 * libeventd-plugin - Library to implement an eventd plugin
 *
 * Copyright © 2011-2016 Quentin "Sardem FF7" Glidic
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
#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct _EventdCoreContext EventdPluginCoreContext;

typedef struct _EventdPluginContext EventdPluginContext;
typedef struct EventdPluginInterface EventdPluginInterface;
typedef struct _EventdPluginAction EventdPluginAction;

typedef enum {
    EVENTD_PLUGIN_COMMAND_STATUS_OK            = 0,
    EVENTD_PLUGIN_COMMAND_STATUS_COMMAND_ERROR = 30,
    EVENTD_PLUGIN_COMMAND_STATUS_EXEC_ERROR    = 31,
    /*
     * Code from 50 to 59 are reserved for the plugins
     * to report a specific status
     */
    EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_1      = 50,
    EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_2      = 51,
    EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_3      = 52,
    EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_4      = 53,
    EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_5      = 54,
    EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_6      = 55,
    EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_7      = 56,
    EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_8      = 57,
    EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_9      = 58,
    EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_10     = 59,
} EventdPluginCommandStatus;

GType eventd_plugin_command_status_get_type(void) G_GNUC_CONST;
#define EVENTD_PLUGIN_TYPE_COMMAND_STATUS (eventd_plugin_command_status_get_type())

/*
 * eventd plugin interface
 */

typedef EventdPluginContext *(*EventdPluginInitFunc)(EventdPluginCoreContext *core);
typedef void (*EventdPluginSimpleFunc)(EventdPluginContext *context);
typedef EventdPluginCommandStatus (*EventdPluginControlCommandFunc)(EventdPluginContext *context, guint64 argc, const gchar * const *argv, gchar **status);
typedef void (*EventdPluginGlobalParseFunc)(EventdPluginContext *context, GKeyFile *key_file);
typedef EventdPluginAction *(*EventdPluginActionParseFunc)(EventdPluginContext *context, GKeyFile *key_file);
typedef void (*EventdPluginEventDispatchFunc)(EventdPluginContext *context, EventdEvent *event);
typedef void (*EventdPluginEventActionFunc)(EventdPluginContext *context, EventdPluginAction *action, EventdEvent *event);

typedef void (*EventdPluginGetInterfaceFunc)(EventdPluginInterface *interface);

void eventd_plugin_interface_add_init_callback(EventdPluginInterface *interface, EventdPluginInitFunc callback);
void eventd_plugin_interface_add_uninit_callback(EventdPluginInterface *interface, EventdPluginSimpleFunc callback);

void eventd_plugin_interface_add_start_callback(EventdPluginInterface *interface, EventdPluginSimpleFunc callback);
void eventd_plugin_interface_add_stop_callback(EventdPluginInterface *interface, EventdPluginSimpleFunc callback);

void eventd_plugin_interface_add_control_command_callback(EventdPluginInterface *interface, EventdPluginControlCommandFunc callback);

void eventd_plugin_interface_add_global_parse_callback(EventdPluginInterface *interface, EventdPluginGlobalParseFunc callback);
void eventd_plugin_interface_add_action_parse_callback(EventdPluginInterface *interface, EventdPluginActionParseFunc callback);
void eventd_plugin_interface_add_config_reset_callback(EventdPluginInterface *interface, EventdPluginSimpleFunc callback);

void eventd_plugin_interface_add_event_dispatch_callback(EventdPluginInterface *interface, EventdPluginEventDispatchFunc callback);
void eventd_plugin_interface_add_event_action_callback(EventdPluginInterface *interface, EventdPluginEventActionFunc callback);


/*
 * eventd core interface
 */

GList *eventd_plugin_core_get_sockets(EventdPluginCoreContext *context, GSocketAddress **binds);

gboolean eventd_plugin_core_push_event(EventdPluginCoreContext *context, EventdEvent *event);

G_END_DECLS

#endif /* __EVENTD_EVENTD_PLUGIN_H__ */
