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

#ifndef __EVENTD_TESTS_UNIT_EVENTD_EVENT_COMMON_H__
#define __EVENTD_TESTS_UNIT_EVENTD_EVENT_COMMON_H__

#include <stdlib.h>
#include <glib.h>
#include <glib-object.h>
#include "libeventd-event.h"
#include "libeventd-event-private.h"
#include "libeventd-protocol.h"

#define EVENTD_EVENT_TEST_UUID "1b4e28ba-2fa1-11d2-883f-b9a761bde3fb"
#define EVENTD_EVENT_TEST_NAME "test-name"
#define EVENTD_EVENT_TEST_CATEGORY "test-category"
#define EVENTD_EVENT_TEST_DATA_NAME "test-name"
#define EVENTD_EVENT_TEST_DATA_CONTENT "test-content"
#define EVENTD_EVENT_TEST_DATA_ESCAPING_NAME "test-name-newline"
#define EVENTD_EVENT_TEST_DATA_ESCAPING_CONTENT "test-content\ntest-newline-content\\test-backslash-context"
#define EVENTD_EVENT_TEST_DATA_ESCAPING_CONTENT_ESCAPED "test-content\\ntest-newline-content\\\\test-backslash-context"

#endif /* __EVENTD_TESTS_UNIT_EVENTD_EVENT_COMMON_H__ */
