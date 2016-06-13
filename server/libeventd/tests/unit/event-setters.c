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

#include "common.h"
#include "event-setters.h"

typedef struct {
    EventdEvent *event;
} SettersData;

static void
_init_data(gpointer fixture, gconstpointer user_data)
{
    SettersData *data = fixture;

    data->event = eventd_event_new_for_uuid_string(EVENTD_EVENT_TEST_UUID, EVENTD_EVENT_TEST_CATEGORY, EVENTD_EVENT_TEST_NAME);
}

static void
_clean_data(gpointer fixture, gconstpointer user_data)
{
    SettersData *data = fixture;

    eventd_event_unref(data->event);
}

static void
_test_new_good_good(gpointer fixture, gconstpointer user_data)
{
    if ( ! g_test_undefined() )
            return;
    if ( g_test_subprocess() )
    {
        eventd_event_new_for_uuid_string(EVENTD_EVENT_TEST_UUID, EVENTD_EVENT_TEST_CATEGORY, EVENTD_EVENT_TEST_NAME);
        exit(0);
    }
    g_test_trap_subprocess(NULL, 0, 0);
    g_test_trap_assert_passed();
}

static void
_test_new_null_good(gpointer fixture, gconstpointer user_data)
{
    if ( ! g_test_undefined() )
            return;
    if ( g_test_subprocess() )
    {
        eventd_event_new_for_uuid_string(EVENTD_EVENT_TEST_UUID, NULL, EVENTD_EVENT_TEST_NAME);
        exit(0);
    }
    g_test_trap_subprocess(NULL, 0, 0);
    g_test_trap_assert_failed();
}

static void
_test_new_good_null(gpointer fixture, gconstpointer user_data)
{
    if ( ! g_test_undefined() )
            return;
    if ( g_test_subprocess() )
    {
        eventd_event_new_for_uuid_string(EVENTD_EVENT_TEST_UUID, NULL, NULL);
        exit(0);
    }
    g_test_trap_subprocess(NULL, 0, 0);
    g_test_trap_assert_failed();
}

static void
_test_add_data_notnull_good_good(gpointer fixture, gconstpointer user_data)
{
    SettersData *data = fixture;

    if ( g_test_subprocess() )
    {
        eventd_event_add_data_string(data->event, EVENTD_EVENT_TEST_DATA_NAME, EVENTD_EVENT_TEST_DATA_CONTENT);
        exit(0);
    }
    g_test_trap_subprocess(NULL, 0, 0);
    g_test_trap_assert_passed();
}

static void
_test_add_data_null_good_good(gpointer fixture, gconstpointer user_data)
{
    if ( ! g_test_undefined() )
            return;
    if ( g_test_subprocess() )
    {
        eventd_event_add_data_string(NULL, EVENTD_EVENT_TEST_DATA_NAME, EVENTD_EVENT_TEST_DATA_CONTENT);
        exit(0);
    }
    g_test_trap_subprocess(NULL, 0, 0);
    g_test_trap_assert_failed();
}

static void
_test_add_data_notnull_bad_good(gpointer fixture, gconstpointer user_data)
{
    SettersData *data = fixture;

    if ( ! g_test_undefined() )
            return;
    if ( g_test_subprocess() )
    {
        eventd_event_add_data_string(data->event, NULL, EVENTD_EVENT_TEST_DATA_CONTENT);
        exit(0);
    }
    g_test_trap_subprocess(NULL, 0, 0);
    g_test_trap_assert_failed();
}

static void
_test_add_data_notnull_good_bad(gpointer fixture, gconstpointer user_data)
{
    SettersData *data = fixture;

    if ( ! g_test_undefined() )
            return;
    if ( g_test_subprocess() )
    {
        eventd_event_add_data_string(data->event, EVENTD_EVENT_TEST_DATA_NAME, NULL);
        exit(0);
    }
    g_test_trap_subprocess(NULL, 0, 0);
    g_test_trap_assert_failed();
}

void
eventd_tests_unit_eventd_event_suite_setters(void)
{
    GTestSuite *suite;

    suite = g_test_create_suite("EventdEvent setters test suite");

    g_test_suite_add(suite, g_test_create_case("new(category, name)",                sizeof(SettersData), NULL, NULL,       _test_new_good_good,                     NULL));
    g_test_suite_add(suite, g_test_create_case("new(NULL, name)",                    sizeof(SettersData), NULL, NULL,       _test_new_null_good,                     NULL));
    g_test_suite_add(suite, g_test_create_case("new(category, NULL)",                sizeof(SettersData), NULL, NULL,       _test_new_good_null,                     NULL));

    g_test_suite_add(suite, g_test_create_case("add_data(event, name, data)",        sizeof(SettersData), NULL, _init_data, _test_add_data_notnull_good_good,        _clean_data));
    g_test_suite_add(suite, g_test_create_case("add_data(NULL,)",                    sizeof(SettersData), NULL, NULL,       _test_add_data_null_good_good,           NULL));
    g_test_suite_add(suite, g_test_create_case("add_data(event, NULL,)",             sizeof(SettersData), NULL, _init_data, _test_add_data_notnull_bad_good,         _clean_data));
    g_test_suite_add(suite, g_test_create_case("add_data(event, NULL, NULL)",        sizeof(SettersData), NULL, _init_data, _test_add_data_notnull_good_bad,         _clean_data));

    g_test_suite_add_suite(g_test_get_root(), suite);
}
