/*
 * eventdctl - Control utility for eventd
 *
 * Copyright © 2011-2013 Quentin "Sardem FF7" Glidic
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

#ifndef __EVENTCTL_H__
#define __EVENTCTL_H__

#include <eventd-plugin.h>

typedef enum {
    EVENTDCTL_RETURN_CODE_OK = 0,
    EVENTDCTL_RETURN_CODE_ARGV_ERROR           = 1,
    EVENTDCTL_RETURN_CODE_CONNECTION_ERROR     = 10,
    EVENTDCTL_RETURN_CODE_INVOCATION_ERROR     = 11,
    EVENTDCTL_RETURN_CODE_COMMAND_ERROR        = 20,
    EVENTDCTL_RETURN_CODE_PLUGIN_ERROR         = 21,
    EVENTDCTL_RETURN_CODE_PLUGIN_COMMAND_ERROR = EVENTD_PLUGIN_COMMAND_STATUS_COMMAND_ERROR,
    EVENTDCTL_RETURN_CODE_PLUGIN_EXEC_ERROR    = EVENTD_PLUGIN_COMMAND_STATUS_EXEC_ERROR,
} EventdctlReturnCode;

#endif /* __EVENTCTL_H__ */
