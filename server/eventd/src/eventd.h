/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2021 Quentin "Sardem FF7" Glidic
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

#ifndef __EVENTD_CORE_H__
#define __EVENTD_CORE_H__

#include "types.h"

typedef enum {
    EVENTD_RETURN_CODE_OK = 0,
    EVENTD_RETURN_CODE_FILESYSTEM_ENCODING_ERROR  = 1,
    EVENTD_RETURN_CODE_ARGV_ERROR                 = 3,
    EVENTD_RETURN_CODE_NO_RUNTIME_DIR_ERROR       = 4,
    EVENTD_RETURN_CODE_CONTROL_INTERFACE_ERROR    = 10,
} EventdReturnCode;

GList *eventd_core_get_binds(EventdCoreContext *context, const gchar *namespace, const gchar * const *binds);
GList *eventd_core_get_sockets(EventdCoreContext *context, const gchar *namespace, GSocketAddress **binds);

const gchar *eventd_core_get_uuid(EventdCoreContext *context);

gboolean eventd_core_push_event(EventdCoreContext *context, EventdEvent *event);

void eventd_core_flags_add(EventdCoreContext *context, GQuark flag);
void eventd_core_flags_remove(EventdCoreContext *context, GQuark flag);
void eventd_core_flags_reset(EventdCoreContext *context);
gchar *eventd_core_flags_list(EventdCoreContext *context);

void eventd_core_config_reload(EventdCoreContext *context);

void eventd_core_stop(EventdCoreContext *context, EventdControlDelayedStop *delayed_stop);

gchar *eventd_core_dump_event(EventdCoreContext *context, const gchar *event_id);
gchar *eventd_core_dump_action(EventdCoreContext *context, const gchar *action_id);

#endif /* __EVENTD_CORE_H__ */
