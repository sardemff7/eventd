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

#ifndef __EVENTD_PLUGINS_RELAY_DNS_SD_H__
#define __EVENTD_PLUGINS_RELAY_DNS_SD_H__

typedef struct _EventdRelayDNSSD EventdRelayDNSSD;

EventdRelayDNSSD *eventd_relay_dns_sd_init(void);
void eventd_relay_dns_sd_uninit(EventdRelayDNSSD *context);

void eventd_relay_dns_sd_start(EventdRelayDNSSD *context);
void eventd_relay_dns_sd_stop(EventdRelayDNSSD *context);

void eventd_relay_dns_sd_monitor_server(EventdRelayDNSSD *context, gchar *name, EventdRelayServer *relay_server);

#endif /* __EVENTD_PLUGINS_RELAY_DNS_SD_H__ */

