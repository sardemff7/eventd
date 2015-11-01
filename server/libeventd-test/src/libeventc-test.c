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

#include <config.h>

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#include <glib.h>
#include <glib/gstdio.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <libeventd-event.h>
#include <libeventc.h>

#define MAX_CONNECTION 3

static GError *error = NULL;
static EventdEvent *event = NULL;
static GMainLoop *loop = NULL;
static guint timeout = 0;

typedef enum {
    STATE_START,
    STATE_SENT,
    STATE_UPDATED,
    STATE_ANSWERED,
    STATE_ENDED
} State;

static struct{
    gssize connection;
    gssize event;
    State state;
} _test_state = {
    .connection = 1,
    .event = 0,
    .state = STATE_START
};

static const gchar *_state_names[] = {
    [STATE_START] = "start",
    [STATE_SENT] = "sent",
    [STATE_UPDATED] = "updated",
    [STATE_ANSWERED] = "answered",
    [STATE_ENDED] = "ended"
};

static gboolean
_check_state(gssize connection, gssize event, State state)
{
    if ( ( connection > -1 ) && ( _test_state.connection != connection ) )
    {
        g_warning("Expected connection %zu, got %zu", connection, _test_state.connection);
        g_main_loop_quit(loop);
        return FALSE;
    }

    if ( ( event > -1 ) && ( _test_state.event != event ) )
    {
        g_warning("[%zu] Expected event %zu, got %zu", _test_state.connection, event, _test_state.event);
        g_main_loop_quit(loop);
        return FALSE;
    }

    if ( ( _test_state.state != state ) )
    {
        g_warning("[%zu, %zu] Expected state '%s', got '%s'", _test_state.connection, _test_state.event, _state_names[state], _state_names[_test_state.state]);
        g_main_loop_quit(loop);
        return FALSE;
    }

    return TRUE;
}

static gboolean
_timeout_callback(gpointer user_data)
{
    g_warning("Test takes too much time, aborting");
    g_main_loop_quit(loop);
    return FALSE;
}

static void _ended_callback(EventdEvent *e, EventdEventEndReason reason, EventcConnection *client);
static void _answered_callback(EventdEvent *e, const gchar *answer, EventcConnection *client);
static void _updated_callback(EventdEvent *e, EventcConnection *client);
static void
_create_event(EventcConnection *client)
{
    event = eventd_event_new("test", "test");

    switch ( ++_test_state.event )
    {
    case 1:
        if ( _test_state.connection == 1 )
            eventd_event_add_data(event, g_strdup("new-test"), g_strdup_printf("Some new\nmessage from %s", g_get_prgname()));
    case 2:
    {
        gchar *tmp = g_strconcat(g_get_prgname(), "-file", NULL );
        eventd_event_add_data(event, g_strdup("file"), g_build_filename(g_getenv("EVENTD_TESTS_TMP_DIR"), tmp, NULL));
        g_free(tmp);
        eventd_event_add_data(event, g_strdup("test"), g_strdup_printf("Some message\nfrom %s", g_get_prgname()));
        eventd_event_add_answer(event,"test");
    }
    case 3:
    default:
    break;
    }

    g_signal_connect(event, "updated", G_CALLBACK(_updated_callback), client);
    g_signal_connect(event, "answered", G_CALLBACK(_answered_callback), client);
    g_signal_connect(event, "ended", G_CALLBACK(_ended_callback), client);

    if ( timeout > 0 )
        g_source_remove(timeout);
    /* The event timeout is 10s, after 11s, there must be something wrong */
    timeout = g_timeout_add_seconds_full(G_PRIORITY_DEFAULT_IDLE, 11, _timeout_callback, NULL, NULL);
}

static void
_connect_callback(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    EventcConnection *client = EVENTC_CONNECTION(obj);

    if ( ! eventc_connection_connect_finish(client, res, &error) )
    {
        g_main_loop_quit(loop);
        return;
    }
    if ( ! _check_state(-1, 0, STATE_START) )
        return;

    switch ( _test_state.connection )
    {
    case 2:
        g_object_unref(event);
    case 1:
        _create_event(client);
        if ( eventc_connection_event(client, event, &error) )
            /* We cheat here by considering the event updated */
            _test_state.state = ( _test_state.connection == 1 ) ? STATE_SENT : STATE_UPDATED;
        else
            g_main_loop_quit(loop);

        return;
    break;
    default:
        g_return_if_reached();
    }
}

static void
_updated_callback(EventdEvent *e, EventcConnection *client)
{
    if ( ! _check_state(1, 1, STATE_SENT) )
        return;

    if ( g_strcmp0(eventd_event_get_data(event, "test"), eventd_event_get_data(event, "new-test")) != 0 )
    {
        g_warning("Event not updated");
        g_main_loop_quit(loop);
        return;
    }

    ++_test_state.state;
}

static void
_answered_callback(EventdEvent *e, const gchar *answer, EventcConnection *client)
{
    if ( ! _check_state(-1, -1, STATE_UPDATED) )
        return;

    if ( g_strcmp0(answer, "test") != 0 )
    {
        g_warning("Wrond answer to event: %s", answer);
        goto fail;
    }

    const gchar *filename = eventd_event_get_data(event, "file");
    gchar *contents;
    if ( ! g_file_get_contents(filename, &contents, NULL, &error) ) goto fail;
    if ( g_strcmp0(eventd_event_get_data(event, "test"), contents) != 0 )
    {
        g_warning("Wrong test file contents: %s", contents);
        g_free(contents);
        goto fail;
    }
    g_free(contents);
    if ( g_unlink(filename) < 0 )
    {
        g_warning("Couldn't remove the file: %s", g_strerror(errno));
        goto fail;
    }

    ++_test_state.state;
    return;
fail:
    g_main_loop_quit(loop);
}

static gboolean
_ended_close_idle_callback(gpointer user_data)
{
    if ( ! _check_state(-1, 3, STATE_ENDED) )
        return FALSE;

    EventcConnection *client = user_data;

    if ( ! eventc_connection_close(client, &error) )
    {
        g_main_loop_quit(loop);
        return FALSE;
    }

    _test_state.event = 0;
    _test_state.state = STATE_START;

    if ( ++_test_state.connection < MAX_CONNECTION )
        eventc_connection_connect(client, _connect_callback, NULL);
    else
        g_main_loop_quit(loop);
    return FALSE;
}

static gboolean
_end_idle_callback(gpointer user_data)
{
    if ( _check_state(-1, 3, STATE_UPDATED) )
    {
        ++_test_state.state;
        eventd_event_end(event, EVENTD_EVENT_END_REASON_CLIENT_DISMISS);
    }
    return FALSE;
}

static void
_ended_callback(EventdEvent *e, EventdEventEndReason reason, EventcConnection *client)
{
    if ( ! _check_state(-1, -1, STATE_ANSWERED) )
        return;
    g_return_if_fail(eventd_event_end_reason_is_valid_value(reason));
    switch ( _test_state.event )
    {
    case 2:
        g_idle_add(_end_idle_callback, NULL);
    case 1:
        if ( reason != EVENTD_EVENT_END_REASON_TEST )
            break;
        g_object_unref(event);
        _create_event(client);
        if ( ! eventc_connection_event(client, event, &error) )
            g_main_loop_quit(loop);
        else
            /* We cheat here by considering the event updated */
            _test_state.state = STATE_UPDATED;
        return;
    case 3:
        if ( reason != EVENTD_EVENT_END_REASON_CLIENT_DISMISS )
            break;
        g_idle_add(_ended_close_idle_callback, client);
        ++_test_state.state;
        return;
    default:
        g_return_if_reached();
    }
    g_warning("[%zu, %zu] Wrong end reason: %s", _test_state.connection, _test_state.event, eventd_event_end_reason_get_value_nick(reason));
    g_main_loop_quit(loop);
}

int
eventd_tests_run_libeventc(const gchar *host)
{
    int r = 0;

    EventcConnection *client;

    client = eventc_connection_new(host, &error);

    if ( client == NULL )
        goto error;

    loop = g_main_loop_new(NULL, FALSE);

    eventc_connection_connect(client, _connect_callback, NULL);

    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    if ( _test_state.connection != MAX_CONNECTION )
    {
        g_warning("Wrong ending state");
        r = 1;
    }

    g_object_unref(event);

    g_object_unref(client);

error:
    if ( error != NULL )
    {
        g_warning("Test failed: %s", error->message);
        r = ( error->domain == EVENTC_ERROR ) ? 2 : 99;
        g_error_free(error);
    }
    return r;
}
