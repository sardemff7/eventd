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

#ifndef __EVENTD_ND_TYPES_H__
#define __EVENTD_ND_TYPES_H__

#include <nkutils-bindings.h>

typedef struct _EventdPluginContext EventdNdContext;
typedef struct _EventdPluginAction EventdNdStyle;
typedef struct _EventdNdNotification EventdNdNotification;
typedef struct _EventdNdQueue EventdNdQueue;

typedef enum {
    EVENTD_ND_ANCHOR_TOP_LEFT,
    EVENTD_ND_ANCHOR_TOP,
    EVENTD_ND_ANCHOR_TOP_RIGHT,
    EVENTD_ND_ANCHOR_BOTTOM_LEFT,
    EVENTD_ND_ANCHOR_BOTTOM,
    EVENTD_ND_ANCHOR_BOTTOM_RIGHT,
    _EVENTD_ND_ANCHOR_SIZE
} EventdNdAnchor;

typedef enum {
    EVENTD_ND_DISMISS_NONE,
    EVENTD_ND_DISMISS_ALL,
    EVENTD_ND_DISMISS_OLDEST,
    EVENTD_ND_DISMISS_NEWEST,
} EventdNdDismissTarget;


#endif /* __EVENTD_ND_TYPES_H__ */
