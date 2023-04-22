/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2021 Quentin "Sardem FF7" Glidic
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

#include <glib.h>

#include <libeventd-event.h>

#include "types.h"
#include "events.h"

#define MAX_DATA 10

typedef struct {
    EventdEvents *events;
} EventdEventsTestFixture;

typedef struct {
    const gchar *name;
    const gchar *content;
} EventdEventsTestEventData;

typedef struct {
    const gchar *category;
    const gchar *name;
    EventdEventsTestEventData data[MAX_DATA + 1];
} EventdEventsTestEvent;

typedef struct {
    const gchar *config;
    EventdEventsTestEvent event;
    const gchar *result;
} EventdEventsTestData;

static const gchar *
_eventd_events_tests_config = ""
"\n[Event test-cat]"
"\nActions=cat action"

"\n[Event test something]"
"\nActions=some action"

"\n[Event test something some-data]"
"\nIfData=some-data"
"\nActions=if data action"

"\n[Event test something some-data-value-string]"
"\nIfData=some-other-data"
"\nIfDataMatches=some-other-data,==,'some-value'"
"\nActions=if data matches string action"

"\n[Event test something some-data-value-integer]"
"\nIfData=some-other-data"
"\nIfDataMatches=some-other-data,==,int64 1000"
"\nActions=if data matches int action"

"\n[Event test something some-data-regex-string]"
"\nIfData=some-other-data"
"\nIfDataRegex=some-other-data,^some-other-value$"
"\nActions=if data regex string action"

"\n[Event test something some-data-regex-integer]"
"\nIfData=some-other-data"
"\nIfDataRegex=some-other-data,^2000$"
"\nActions=if data regex int action"
"";

static const struct {
    const gchar *testpath;
    EventdEventsTestData data;
} _eventd_events_tests_list[] = {
    {
        .testpath = "/eventd/events/category/match",
        .data = {
            .event = {
                .category = "test-cat",
                .name = "something",
                .data = {
                    { .name = NULL }
                }
            },
            .result = "cat action"
        }
    },
    {
        .testpath = "/eventd/events/category/no-match",
        .data = {
            .event = {
                .category = "not-test",
                .name = "something",
                .data = {
                    { .name = NULL }
                }
            },
            .result = NULL
        }
    },
    {
        .testpath = "/eventd/events/name/match",
        .data = {
            .event = {
                .category = "test",
                .name = "something",
                .data = {
                    { .name = NULL }
                }
            },
            .result = "some action"
        }
    },
    {
        .testpath = "/eventd/events/name/no-match",
        .data = {
            .event = {
                .category = "test",
                .name = "something-else",
                .data = {
                    { .name = NULL }
                }
            },
            .result = NULL
        }
    },
    {
        .testpath = "/eventd/events/if-data/match",
        .data = {
            .event = {
                .category = "test",
                .name = "something",
                .data = {
                    { .name = "some-data", .content = "false" },
                    { .name = NULL }
                }
            },
            .result = "if data action"
        }
    },
    {
        .testpath = "/eventd/events/if-data/fallback",
        .data = {
            .event = {
                .category = "test",
                .name = "something",
                .data = {
                    { .name = NULL }
                }
            },
            .result = "some action"
        }
    },
    {
        .testpath = "/eventd/events/if-data-matches/match/string",
        .data = {
            .event = {
                .category = "test",
                .name = "something",
                .data = {
                    { .name = "some-other-data", .content = "\"some-value\"" },
                    { .name = NULL }
                }
            },
            .result = "if data matches string action"
        }
    },
    {
        .testpath = "/eventd/events/if-data-matches/match/integer",
        .data = {
            .event = {
                .category = "test",
                .name = "something",
                .data = {
                    { .name = "some-other-data", .content = "int64 1000" },
                    { .name = NULL }
                }
            },
            .result = "if data matches int action"
        }
    },
    {
        .testpath = "/eventd/events/if-data-matches/fallback",
        .data = {
            .event = {
                .category = "test",
                .name = "something",
                .data = {
                    { .name = NULL }
                }
            },
            .result = "some action"
        }
    },
    {
        .testpath = "/eventd/events/if-data-regex/match",
        .data = {
            .event = {
                .category = "test",
                .name = "something",
                .data = {
                    { .name = "some-other-data", .content = "'some-other-value'" },
                    { .name = NULL }
                }
            },
            .result = "if data regex string action"
        }
    },
    {
        .testpath = "/eventd/events/if-data-regex/fallback/integer",
        .data = {
            .event = {
                .category = "test",
                .name = "something",
                .data = {
                    { .name = "some-other-data", .content = "2000" },
                    { .name = NULL }
                }
            },
            .result = "some action"
        }
    },
    {
        .testpath = "/eventd/events/if-data-regex/fallback/null",
        .data = {
            .event = {
                .category = "test",
                .name = "something",
                .data = {
                    { .name = NULL }
                }
            },
            .result = "some action"
        }
    },
};

static void
_init_data(EventdEventsTestFixture *fixture, gconstpointer user_data)
{
    EventdEventsTestFixture *data = fixture;

    GKeyFile *key_file = g_key_file_new();
    g_key_file_load_from_data(key_file, _eventd_events_tests_config, -1, G_KEY_FILE_NONE, NULL);

    data->events = eventd_events_new();
    eventd_events_parse(data->events, key_file);

    g_key_file_unref(key_file);
}

static void
_clean_data(EventdEventsTestFixture *fixture, gconstpointer user_data)
{
    EventdEventsTestFixture *data = fixture;

    eventd_events_free(data->events);
}

static void
_eventd_events_tests_func(EventdEventsTestFixture *fixture, gconstpointer user_data)
{
    EventdEventsTestData *data = (EventdEventsTestData *) user_data;

    EventdEvent *event = eventd_event_new(data->event.category, data->event.name);
    EventdEventsTestEventData *event_data = data->event.data;
    for ( event_data = data->event.data ; event_data->name != NULL ; ++event_data )
        eventd_event_add_data(event, g_strdup(event_data->name), g_variant_parse(NULL, event_data->content, NULL, NULL, NULL));

    const GList *result = NULL;
    GList fake_result = { .data = NULL };
    eventd_events_process_event(fixture->events, event, NULL, &result);
    eventd_event_unref(event);

    if ( data->result == NULL )
        g_assert_null(result);
    else
    {
        g_assert_nonnull(result);
        if ( result == NULL )
            result = &fake_result;
        g_assert_cmpstr(result->data, ==, data->result);
    }
}

void
eventd_tests_add_events_suite()
{
    gsize i;
    for ( i = 0 ; i < G_N_ELEMENTS(_eventd_events_tests_list) ; ++i )
        g_test_add(_eventd_events_tests_list[i].testpath, EventdEventsTestFixture, &_eventd_events_tests_list[i].data, _init_data, _eventd_events_tests_func, _clean_data);
}
