/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2015 Quentin "Sardem FF7" Glidic
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
 * MERCHANTABILITY or FITNESS FOR A LINEICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with eventd. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "common.h"
#include "protocol-generator.h"

typedef struct {
    EventdProtocol *protocol;
    EventdEvent *event;
} GeneratorData;

static void
_init_data(gpointer fixture, gconstpointer user_data)
{
    GeneratorData *data = fixture;
    gboolean with_answer = GPOINTER_TO_INT(user_data);

    uuid_t uuid;
    uuid_parse(EVENTD_EVENT_TEST_UUID, uuid);

    data->protocol = eventd_protocol_evp_new();
    data->event = eventd_event_new_for_uuid(uuid, EVENTD_EVENT_TEST_NAME, EVENTD_EVENT_TEST_NAME);

    if ( with_answer )
        eventd_event_add_answer(data->event, EVENTD_EVENT_TEST_ANSWER);
}

static void
_init_data_with_data(gpointer fixture, gconstpointer user_data)
{
    GeneratorData *data = fixture;
    _init_data(fixture, user_data);

    eventd_event_add_data(data->event, g_strdup(EVENTD_EVENT_TEST_DATA_NAME), g_strdup(EVENTD_EVENT_TEST_DATA_CONTENT));
    eventd_event_add_data(data->event, g_strdup(EVENTD_EVENT_TEST_DATA_NEWLINE_NAME ), g_strdup(EVENTD_EVENT_TEST_DATA_NEWLINE_CONTENT));
    eventd_event_add_answer_data(data->event, g_strdup(EVENTD_EVENT_TEST_DATA_NAME), g_strdup(EVENTD_EVENT_TEST_DATA_CONTENT));
    eventd_event_add_answer_data(data->event, g_strdup(EVENTD_EVENT_TEST_DATA_NEWLINE_NAME ), g_strdup(EVENTD_EVENT_TEST_DATA_NEWLINE_CONTENT));
}

static void
_clean_data(gpointer fixture, gconstpointer user_data)
{
    GeneratorData *data = fixture;

    g_object_unref(data->event);
}

static void
_test_evp_generate_passive(gpointer fixture, gconstpointer user_data)
{
    GeneratorData *data = fixture;
    const gchar *expected = "PASSIVE\n";
    gchar *message;

    message = eventd_protocol_generate_passive(data->protocol);
    g_assert_cmpstr(message, ==, expected);

    g_free(message);
}

static void
_test_evp_generate_event(gpointer fixture, gconstpointer user_data)
{
    GeneratorData *data = fixture;
    gboolean with_answer = GPOINTER_TO_INT(user_data);
    const gchar *expected;
    gchar *message;


    if ( eventd_event_get_all_data(data->event) != NULL )
    {
        if ( with_answer )
        {
            expected =
                ".EVENT " EVENTD_EVENT_TEST_UUID " " EVENTD_EVENT_TEST_NAME " " EVENTD_EVENT_TEST_NAME "\n"
                "ANSWER " EVENTD_EVENT_TEST_ANSWER "\n"
                "DATA " EVENTD_EVENT_TEST_DATA_NAME " " EVENTD_EVENT_TEST_DATA_CONTENT "\n"
                ".DATA " EVENTD_EVENT_TEST_DATA_NEWLINE_NAME "\n" EVENTD_EVENT_TEST_DATA_NEWLINE_CONTENT "\n.\n"
                ".\n";
        }
        else
        {
            expected =
                ".EVENT " EVENTD_EVENT_TEST_UUID " " EVENTD_EVENT_TEST_NAME " " EVENTD_EVENT_TEST_NAME "\n"
                "DATA " EVENTD_EVENT_TEST_DATA_NAME " " EVENTD_EVENT_TEST_DATA_CONTENT "\n"
                ".DATA " EVENTD_EVENT_TEST_DATA_NEWLINE_NAME "\n" EVENTD_EVENT_TEST_DATA_NEWLINE_CONTENT "\n.\n"
                ".\n";
        }
    }
    else
    {
        if ( with_answer )
        {
            expected =
                ".EVENT " EVENTD_EVENT_TEST_UUID " " EVENTD_EVENT_TEST_NAME " " EVENTD_EVENT_TEST_NAME "\n"
                "ANSWER " EVENTD_EVENT_TEST_ANSWER "\n"
                ".\n";
        }
        else
            expected = "EVENT " EVENTD_EVENT_TEST_UUID " " EVENTD_EVENT_TEST_NAME " " EVENTD_EVENT_TEST_NAME "\n";
    }

    message = eventd_protocol_generate_event(data->protocol, data->event);
    g_assert_cmpstr(message, ==, expected);

    g_free(message);
}

static void
_test_evp_generate_answered(gpointer fixture, gconstpointer user_data)
{
    GeneratorData *data = fixture;
    const gchar *expected;
    gchar *message;

    if ( eventd_event_get_all_answer_data(data->event) != NULL )
    {
        expected =
            ".ANSWERED " EVENTD_EVENT_TEST_UUID " " EVENTD_EVENT_TEST_ANSWER "\n"
            "DATA " EVENTD_EVENT_TEST_DATA_NAME " " EVENTD_EVENT_TEST_DATA_CONTENT "\n"
            ".DATA " EVENTD_EVENT_TEST_DATA_NEWLINE_NAME "\n" EVENTD_EVENT_TEST_DATA_NEWLINE_CONTENT "\n.\n"
            ".\n";
    }
    else
        expected = "ANSWERED " EVENTD_EVENT_TEST_UUID " " EVENTD_EVENT_TEST_ANSWER "\n";

    message = eventd_protocol_generate_answered(data->protocol, data->event, EVENTD_EVENT_TEST_ANSWER);
    g_assert_cmpstr(message, ==, expected);

    g_free(message);
}

static void
_test_evp_generate_ended(gpointer fixture, gconstpointer user_data)
{
    GeneratorData *data = fixture;
    const gchar *expected = "ENDED " EVENTD_EVENT_TEST_UUID " timeout\n";
    gchar *message;

    message = eventd_protocol_generate_ended(data->protocol, data->event, EVENTD_EVENT_END_REASON_TIMEOUT);
    g_assert_cmpstr(message, ==, expected);

    g_free(message);
}

static void
_test_evp_generate_bye(gpointer fixture, gconstpointer user_data)
{
    GeneratorData *data = fixture;
    gboolean with_message = GPOINTER_TO_INT(user_data);
    const gchar *expected;
    const gchar *bye_message = NULL;
    gchar *message;

    if ( with_message )
    {
        bye_message = "test";
        expected = "BYE test\n";
    }
    else
        expected = "BYE\n";

    message = eventd_protocol_generate_bye(data->protocol, bye_message);
        g_assert_cmpstr(message, ==, expected);

    g_free(message);
}

void
eventd_tests_unit_eventd_protocol_suite_generator(void)
{
    GTestSuite *suite;

    suite = g_test_create_suite("LibeventdEvp generator test suite");

    g_test_suite_add(suite, g_test_create_case("evp_generate_passive()",           sizeof(GeneratorData), NULL,                  _init_data,           _test_evp_generate_passive,  _clean_data));
    g_test_suite_add(suite, g_test_create_case("evp_generate_event()",             sizeof(GeneratorData), NULL,                  _init_data,           _test_evp_generate_event,    _clean_data));
    g_test_suite_add(suite, g_test_create_case("evp_generate_event(data)",         sizeof(GeneratorData), NULL,                  _init_data_with_data, _test_evp_generate_event,    _clean_data));
    g_test_suite_add(suite, g_test_create_case("evp_generate_event(answer, data)", sizeof(GeneratorData), GINT_TO_POINTER(TRUE), _init_data_with_data, _test_evp_generate_event,    _clean_data));
    g_test_suite_add(suite, g_test_create_case("evp_generate_event(answer)",       sizeof(GeneratorData), GINT_TO_POINTER(TRUE), _init_data,           _test_evp_generate_event,    _clean_data));
    g_test_suite_add(suite, g_test_create_case("evp_generate_answered()",          sizeof(GeneratorData), NULL,                  _init_data,           _test_evp_generate_answered, _clean_data));
    g_test_suite_add(suite, g_test_create_case("evp_generate_answered(data)",      sizeof(GeneratorData), NULL,                  _init_data_with_data, _test_evp_generate_answered, _clean_data));
    g_test_suite_add(suite, g_test_create_case("evp_generate_ended()",             sizeof(GeneratorData), NULL,                  _init_data,           _test_evp_generate_ended,    _clean_data));
    g_test_suite_add(suite, g_test_create_case("evp_generate_bye()",               sizeof(GeneratorData), NULL,                  _init_data,           _test_evp_generate_bye,      _clean_data));
    g_test_suite_add(suite, g_test_create_case("evp_generate_bye(message)",        sizeof(GeneratorData), GINT_TO_POINTER(TRUE), _init_data,           _test_evp_generate_bye,      _clean_data));

    g_test_suite_add_suite(g_test_get_root(), suite);
}
