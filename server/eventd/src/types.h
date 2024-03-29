/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2024 Morgane "Sardem FF7" Glidic
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

#ifndef __EVENTD_TYPES_H__
#define __EVENTD_TYPES_H__

#include "libeventd-event.h"
#include "eventd-plugin.h"
#include "eventdctl.h"

typedef struct _EventdCoreContext EventdCoreContext;

typedef struct _EventdControl EventdControl;
typedef struct _EventdControlDelayedStop EventdControlDelayedStop;
typedef struct _EventdConfig EventdConfig;
typedef struct _EventdEvents EventdEvents;
typedef struct _EventdActions EventdActions;
typedef struct _EventdSockets EventdSockets;

#endif /* __EVENTD_TYPES_H__ */
