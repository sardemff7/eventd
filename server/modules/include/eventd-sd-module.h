/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2016 Quentin "Sardem FF7" Glidic
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

#ifndef __EVENTD_SD_MODULE_H__
#define __EVENTD_SD_MODULE_H__

#include <libeventd-event.h>

typedef struct _EventdRelayServer EventdRelayServer;

typedef enum {
    EVENTD_SD_MODULE_NONE = 0,
    EVENTD_SD_MODULE_DNS_SD,
    EVENTD_SD_MODULE_SSDP,
    _EVENTD_SD_MODULES_SIZE
} EventdSdModules;

typedef struct {
    gboolean (*server_has_address)(EventdRelayServer *server);
    void (*server_set_address)(EventdRelayServer *server, GSocketConnectable *address);
    void (*server_start)(EventdRelayServer *server);
    void (*server_stop)(EventdRelayServer *server);
} EventdSdModuleControlInterface;

typedef struct _EventdSdModuleContext EventdSdModuleContext;

typedef struct {
    EventdSdModuleContext *(*init)(const EventdSdModuleControlInterface *control);
    void (*uninit)(EventdSdModuleContext *context);

    void (*set_publish_name)(EventdSdModuleContext *context, const gchar *publish_name);
    void (*monitor_server)(EventdSdModuleContext *context, const gchar *discover_name, EventdRelayServer *server);

    void (*start)(EventdSdModuleContext *context, GList *sockets);
    void (*stop)(EventdSdModuleContext *context);

    gpointer module;
    EventdSdModuleContext *context;
} EventdSdModule;

typedef void (*EventdSdModuleGetInfoFunc)(EventdSdModule *backend);
void eventd_sd_module_get_info(EventdSdModule *backend);

#endif /* __EVENTD_SD_MODULE_H__ */
