/*
 * eventdctl - Control utility for eventd
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

#ifndef __EVENTCTL_H__
#define __EVENTCTL_H__

#include "eventd-plugin.h"

typedef enum {
    EVENTDCTL_RETURN_CODE_OK = 0,
    EVENTDCTL_RETURN_CODE_ARGV_ERROR           = 1,
    EVENTDCTL_RETURN_CODE_CONNECTION_ERROR     = 10,
    EVENTDCTL_RETURN_CODE_INVOCATION_ERROR     = 11,
    EVENTDCTL_RETURN_CODE_COMMAND_ERROR        = 20,
    EVENTDCTL_RETURN_CODE_PLUGIN_ERROR         = 21,
    EVENTDCTL_RETURN_CODE_PLUGIN_COMMAND_ERROR = EVENTD_PLUGIN_COMMAND_STATUS_COMMAND_ERROR,
    EVENTDCTL_RETURN_CODE_PLUGIN_EXEC_ERROR    = EVENTD_PLUGIN_COMMAND_STATUS_EXEC_ERROR,
    EVENTDCTL_RETURN_CODE_PLUGIN_CUSTOM_1      = EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_1,
    EVENTDCTL_RETURN_CODE_PLUGIN_CUSTOM_2      = EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_2,
    EVENTDCTL_RETURN_CODE_PLUGIN_CUSTOM_3      = EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_3,
    EVENTDCTL_RETURN_CODE_PLUGIN_CUSTOM_4      = EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_4,
    EVENTDCTL_RETURN_CODE_PLUGIN_CUSTOM_5      = EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_5,
    EVENTDCTL_RETURN_CODE_PLUGIN_CUSTOM_6      = EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_6,
    EVENTDCTL_RETURN_CODE_PLUGIN_CUSTOM_7      = EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_7,
    EVENTDCTL_RETURN_CODE_PLUGIN_CUSTOM_8      = EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_8,
    EVENTDCTL_RETURN_CODE_PLUGIN_CUSTOM_9      = EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_9,
    EVENTDCTL_RETURN_CODE_PLUGIN_CUSTOM_10     = EVENTD_PLUGIN_COMMAND_STATUS_CUSTOM_10,
} EventdctlReturnCode;

#endif /* __EVENTCTL_H__ */
