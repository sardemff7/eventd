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

static const gchar *PARSER_PASSIVE_LINES[] = {
    "PASSIVE",
    "EVENT " EVENTD_EVENT_TEST_UUID " " EVENTD_EVENT_TEST_NAME " " EVENTD_EVENT_TEST_NAME,
};

static const gchar *BUILDER_EVENT_LINES[] = {
    ".EVENT " EVENTD_EVENT_TEST_UUID " " EVENTD_EVENT_TEST_NAME " " EVENTD_EVENT_TEST_NAME,
    "DATA " EVENTD_EVENT_TEST_DATA_NAME " " EVENTD_EVENT_TEST_DATA_CONTENT,
    ".DATA " EVENTD_EVENT_TEST_DATA_NAME,
    ".." EVENTD_EVENT_TEST_DATA_CONTENT,
    EVENTD_EVENT_TEST_DATA_CONTENT,
    ".",
    "."
};

typedef struct {
    EventdProtocol *protocol;
    EventdEvent *event;
} EvpData;

static void
_test_evp_event_callback(EventdProtocolEvp *parser, EventdEvent *event, gpointer user_data)
{
    EvpData *data = user_data;

    data->event = g_object_ref(event);
}

static void
_init_data_evp(gpointer fixture, gconstpointer user_data)
{
    EvpData *data = fixture;

    data->protocol = eventd_protocol_evp_new();

    g_signal_connect(data->protocol, "event", G_CALLBACK(_test_evp_event_callback), data);
}

static void
_clean_data_evp(gpointer fixture, gconstpointer user_data)
{
    EvpData *data = fixture;

    if ( data->event != NULL )
        g_object_unref(data->event);

    g_object_unref(data->protocol);
}

static void
_test_evp_parse_passive_good(gpointer fixture, gconstpointer user_data)
{
    EvpData *data = fixture;
    gboolean r;
    GError *error = NULL;
    gsize i, last = G_N_ELEMENTS(PARSER_PASSIVE_LINES) - 1;
    gchar *line;

    for ( i = 0 ; i < last ; ++i )
    {
        line = g_strdup(PARSER_PASSIVE_LINES[i]);
        r = eventd_protocol_parse(data->protocol, &line, &error);
        g_assert_true(r);
        g_assert_no_error(error);
        g_assert_null(line);
        g_assert_null(data->event);
    }

    line = g_strdup(PARSER_PASSIVE_LINES[G_N_ELEMENTS(PARSER_PASSIVE_LINES) - 1]);
    r = eventd_protocol_parse(data->protocol, &line, &error);
    g_assert_true(r);
    g_assert_no_error(error);
    g_assert_null(line);
    g_assert_nonnull(data->event);
    g_assert_true(EVENTD_IS_EVENT(data->event));
}

static void
_test_evp_parse_passive_bad(gpointer fixture, gconstpointer user_data)
{
    EvpData *data = fixture;
    gboolean r;
    GError *error = NULL;
    gchar *line;

    _test_evp_parse_passive_good(fixture, user_data);

    line = g_strdup(PARSER_PASSIVE_LINES[0]);
    r = eventd_protocol_parse(data->protocol, &line, &error);
    g_assert_false(r);
    g_assert_error(error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_UNEXPECTED_TOKEN);
    g_assert_null(line);
}

static void
_test_evp_parse_parts_good(gpointer fixture, gconstpointer user_data)
{
    EvpData *data = fixture;
    gboolean r;
    GError *error = NULL;
    gsize i, last = G_N_ELEMENTS(BUILDER_EVENT_LINES) - 1;
    gchar *line;

    for ( i = 0 ; i < last ; ++i )
    {
        line = g_strdup(BUILDER_EVENT_LINES[i]);
        r = eventd_protocol_parse(data->protocol, &line, &error);
        g_assert_true(r);
        g_assert_no_error(error);
        g_assert_null(line);
        g_assert_null(data->event);
    }

    line = g_strdup(BUILDER_EVENT_LINES[G_N_ELEMENTS(BUILDER_EVENT_LINES) - 1]);
    r = eventd_protocol_parse(data->protocol, &line, &error);
    g_assert_true(r);
    g_assert_no_error(error);
    g_assert_null(line);
    g_assert_nonnull(data->event);
    g_assert_true(EVENTD_IS_EVENT(data->event));
}

static void
_test_evp_parse_parts_bad(gpointer fixture, gconstpointer user_data)
{
    EvpData *data = fixture;
    gboolean r;
    GError *error = NULL;
    gsize i, last = G_N_ELEMENTS(BUILDER_EVENT_LINES) - 1;
    gchar *line;

    for ( i = 0 ; i < last ; ++i )
    {
        line = g_strdup(BUILDER_EVENT_LINES[i]);
        r = eventd_protocol_parse(data->protocol, &line, &error);
        g_assert_true(r);
        g_assert_no_error(error);
        g_assert_null(line);
        g_assert_null(data->event);
    }

    line = g_strdup(BUILDER_EVENT_LINES[0]);
    r = eventd_protocol_parse(data->protocol, &line, &error);
    g_assert_false(r);
    g_assert_error(error, EVENTD_PROTOCOL_PARSE_ERROR, EVENTD_PROTOCOL_PARSE_ERROR_UNEXPECTED_TOKEN);
    g_assert_null(line);
    g_assert_null(data->event);
}

void
eventd_tests_unit_eventd_protocol_suite_parser(void)
{
    GTestSuite *suite;

    suite = g_test_create_suite("LibeventdEvp parser test suite");

    g_test_suite_add(suite, g_test_create_case("evp_parse(passive_good)", sizeof(EvpData), NULL, _init_data_evp, _test_evp_parse_passive_good, _clean_data_evp));
    g_test_suite_add(suite, g_test_create_case("evp_parse(passive_bad)",  sizeof(EvpData), NULL, _init_data_evp, _test_evp_parse_passive_bad,  _clean_data_evp));
    g_test_suite_add(suite, g_test_create_case("evp_parse(parts_good)",   sizeof(EvpData), NULL, _init_data_evp, _test_evp_parse_parts_good,   _clean_data_evp));
    g_test_suite_add(suite, g_test_create_case("evp_parse(parts_bad)",    sizeof(EvpData), NULL, _init_data_evp, _test_evp_parse_parts_bad,    _clean_data_evp));

    g_test_suite_add_suite(g_test_get_root(), suite);
}
