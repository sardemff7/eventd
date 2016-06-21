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

#ifndef __EVENTD_EVP_EVP_H__
#define __EVENTD_EVP_EVP_H__

typedef struct _EventdEvpContext EventdEvpContext;

EventdEvpContext *eventd_evp_init(EventdCoreContext *core, const gchar * const *binds, GList **user_sockets);
void eventd_evp_uninit(EventdEvpContext *evp);

void eventd_evp_start(EventdEvpContext *evp);
void eventd_evp_stop(EventdEvpContext *evp);

void eventd_evp_global_parse(EventdEvpContext *evp, GKeyFile *config_file);
void eventd_evp_config_reset(EventdEvpContext *evp);

void eventd_evp_event_dispatch(EventdEvpContext *evp, EventdEvent *event);

#endif /* __EVENTD_EVP_EVP_H__ */
