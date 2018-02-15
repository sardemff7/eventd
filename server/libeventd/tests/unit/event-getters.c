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

#include "common.h"
#include "event-getters.h"

typedef struct {
    EventdEvent *event;
} GettersData;

static void
_init_data(gpointer fixture, gconstpointer user_data)
{
    GettersData *data = fixture;

    data->event = eventd_event_new_for_uuid_string(EVENTD_EVENT_TEST_UUID, EVENTD_EVENT_TEST_CATEGORY, EVENTD_EVENT_TEST_NAME);
}

static void
_init_data_with_data(gpointer fixture, gconstpointer user_data)
{
    GettersData *data = fixture;
    _init_data(fixture, user_data);

    eventd_event_add_data_string(data->event, g_strdup(EVENTD_EVENT_TEST_DATA_NAME), g_strdup(EVENTD_EVENT_TEST_DATA_CONTENT));
}

static void
_clean_data(gpointer fixture, gconstpointer user_data)
{
    GettersData *data = fixture;

    eventd_event_unref(data->event);
}

static void
_test_get_uuid_notnull(gpointer fixture, gconstpointer user_data)
{
    GettersData *data = fixture;

    g_assert_cmpstr(eventd_event_get_uuid(data->event), ==, EVENTD_EVENT_TEST_UUID);
}

static void
_test_get_uuid_null(gpointer fixture, gconstpointer user_data)
{
    if ( ! g_test_undefined() )
            return;
    if ( g_test_subprocess() )
    {
        eventd_event_get_uuid(NULL);
        exit(0);
    }
    g_test_trap_subprocess(NULL, 0, 0);
    g_test_trap_assert_failed();
}

static void
_test_get_category_notnull(gpointer fixture, gconstpointer user_data)
{
    GettersData *data = fixture;

    g_assert_cmpstr(eventd_event_get_category(data->event), ==, EVENTD_EVENT_TEST_CATEGORY);
}

static void
_test_get_category_null(gpointer fixture, gconstpointer user_data)
{
    if ( ! g_test_undefined() )
            return;
    if ( g_test_subprocess() )
    {
        eventd_event_get_category(NULL);
        exit(0);
    }
    g_test_trap_subprocess(NULL, 0, 0);
    g_test_trap_assert_failed();
}

static void
_test_get_name_null(gpointer fixture, gconstpointer user_data)
{
    if ( ! g_test_undefined() )
            return;
    if ( g_test_subprocess() )
    {
        eventd_event_get_name(NULL);
        exit(0);
    }
    g_test_trap_subprocess(NULL, 0, 0);
    g_test_trap_assert_failed();
}

static void
_test_get_name_notnull(gpointer fixture, gconstpointer user_data)
{
    GettersData *data = fixture;

    g_assert_cmpstr(eventd_event_get_name(data->event), ==, EVENTD_EVENT_TEST_NAME);
}

static void
_test_get_data_notnull_good__null(gpointer fixture, gconstpointer user_data)
{
    GettersData *data = fixture;

    g_assert(eventd_event_get_data_string(data->event, EVENTD_EVENT_TEST_DATA_NAME) == NULL);
}

static void
_test_get_data_notnull_good2__null(gpointer fixture, gconstpointer user_data)
{
    GettersData *data = fixture;

    g_assert(eventd_event_get_data_string(data->event, EVENTD_EVENT_TEST_DATA_NAME"-bad") == NULL);
}

static void
_test_get_data_notnull_good__notnull(gpointer fixture, gconstpointer user_data)
{
    GettersData *data = fixture;

    g_assert_cmpstr(eventd_event_get_data_string(data->event, EVENTD_EVENT_TEST_DATA_NAME), ==, EVENTD_EVENT_TEST_DATA_CONTENT);
}

static void
_test_get_data_null_good__null(gpointer fixture, gconstpointer user_data)
{
    if ( ! g_test_undefined() )
            return;
    if ( g_test_subprocess() )
    {
        eventd_event_get_data_string(NULL, EVENTD_EVENT_TEST_DATA_NAME);
        exit(0);
    }
    g_test_trap_subprocess(NULL, 0, 0);
    g_test_trap_assert_failed();
}

static void
_test_get_data_notnull_bad__null(gpointer fixture, gconstpointer user_data)
{
    GettersData *data = fixture;

    if ( ! g_test_undefined() )
            return;
    if ( g_test_subprocess() )
    {
        eventd_event_get_data_string(data->event, NULL);
        exit(0);
    }
    g_test_trap_subprocess(NULL, 0, 0);
    g_test_trap_assert_failed();
}

void
eventd_tests_unit_eventd_event_suite_getters(void)
{
    GTestSuite *suite;

    suite = g_test_create_suite("EventdEvent getters test suite");

    g_test_suite_add(suite, g_test_create_case("get_uuid(event)",                        sizeof(GettersData), NULL, _init_data,           _test_get_uuid_notnull,                      _clean_data));
    g_test_suite_add(suite, g_test_create_case("get_uuid(NULL)",                         sizeof(GettersData), NULL, NULL,                 _test_get_uuid_null,                         NULL));

    g_test_suite_add(suite, g_test_create_case("get_category(event)",                    sizeof(GettersData), NULL, _init_data,           _test_get_category_notnull,                  _clean_data));
    g_test_suite_add(suite, g_test_create_case("get_category(NULL)",                     sizeof(GettersData), NULL, NULL,                 _test_get_category_null,                     NULL));

    g_test_suite_add(suite, g_test_create_case("get_name(event)",                        sizeof(GettersData), NULL, _init_data,           _test_get_name_notnull,                      _clean_data));
    g_test_suite_add(suite, g_test_create_case("get_name(NULL)",                         sizeof(GettersData), NULL, NULL      ,           _test_get_name_null,                         NULL));

    g_test_suite_add(suite, g_test_create_case("get_data(event, name) = NULL",           sizeof(GettersData), NULL, _init_data,           _test_get_data_notnull_good__null,           _clean_data));
    g_test_suite_add(suite, g_test_create_case("get_data(event, name2) = NULL",          sizeof(GettersData), NULL, _init_data_with_data, _test_get_data_notnull_good2__null,          _clean_data));
    g_test_suite_add(suite, g_test_create_case("get_data(event, name) = content",        sizeof(GettersData), NULL, _init_data_with_data, _test_get_data_notnull_good__notnull,        _clean_data));
    g_test_suite_add(suite, g_test_create_case("get_data(NULL)",                         sizeof(GettersData), NULL, NULL,                 _test_get_data_null_good__null,              NULL));
    g_test_suite_add(suite, g_test_create_case("get_data(event, NULL)",                  sizeof(GettersData), NULL, NULL,                 _test_get_data_notnull_bad__null,            NULL));

    g_test_suite_add_suite(g_test_get_root(), suite);
}

