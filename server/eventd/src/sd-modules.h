/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2018 Quentin "Sardem FF7" Glidic
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

#ifndef __EVENTD_SD_MODULES_H__
#define __EVENTD_SD_MODULES_H__

#include "eventd-sd-module.h"

void eventd_sd_modules_load(const EventdSdModuleControlInterface *control, GList *sockets);
void eventd_sd_modules_unload(void);

void eventd_sd_modules_set_publish_name(const gchar *name);
void eventd_sd_modules_monitor_server(const gchar *name, EventdRelayServer *server);

void eventd_sd_modules_start(void);
void eventd_sd_modules_stop(void);

gboolean eventd_sd_modules_can_discover(void);

#endif /* __EVENTD_SD_MODULES_H__ */
