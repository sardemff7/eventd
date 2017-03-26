/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2017 Quentin "Sardem FF7" Glidic
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

#ifndef __EVENTD_RELAY_RELAY_H__
#define __EVENTD_RELAY_RELAY_H__

typedef struct _EventdRelayContext EventdRelayContext;

EventdRelayContext *eventd_relay_init(EventdCoreContext *core);
void eventd_relay_uninit(EventdRelayContext *evp);

void eventd_relay_start(EventdRelayContext *evp);
void eventd_relay_stop(EventdRelayContext *evp);

EventdPluginCommandStatus eventd_relay_control_command(EventdRelayContext *context, guint64 argc, const gchar * const *argv, gchar **status);

void eventd_relay_global_parse(EventdRelayContext *evp, GKeyFile *config_file);
void eventd_relay_config_reset(EventdRelayContext *evp);

void eventd_relay_set_certificate(EventdRelayContext *relay, GTlsCertificate *certificate);

void eventd_relay_event_dispatch(EventdRelayContext *evp, EventdEvent *event);

#endif /* __EVENTD_RELAY_RELAY_H__ */
