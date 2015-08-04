/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2012 Quentin "Sardem FF7" Glidic
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
#include <glib-compat.h>
#include <glib-object.h>
#include <libeventd-event.h>

#define EVENTD_EVENT_TEST_NAME "test-name"
#define EVENTD_EVENT_TEST_CATEGORY "test-category"
#define EVENTD_EVENT_TEST_TIMEOUT -1
#define EVENTD_EVENT_TEST_ANSWER "test-answer"
#define EVENTD_EVENT_TEST_DATA_NAME "test-name"
#define EVENTD_EVENT_TEST_DATA_CONTENT "test-content"

#endif /* __EVENTD_TESTS_UNIT_EVENTD_EVENT_COMMON_H__ */
