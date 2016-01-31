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
    STATE_END,
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
    [STATE_END] = "end",
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

static void
_create_event(EventcConnection *client)
{
    event = eventd_event_new("test", "test");

    switch ( ++_test_state.event )
    {
    case 1:
    case 2:
    {
        gchar *tmp = g_strconcat(g_get_prgname(), "-file", NULL );
        eventd_event_add_data(event, g_strdup("file"), g_build_filename(g_getenv("EVENTD_TESTS_TMP_DIR"), tmp, NULL));
        g_free(tmp);
        eventd_event_add_data(event, g_strdup("test"), g_strdup_printf("Some message\nfrom %s", g_get_prgname()));
    }
    case 3:
    default:
    break;
    }

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
        eventd_event_unref(event);
    case 1:
        _create_event(client);
        if ( eventc_connection_event(client, event, &error) )
            _test_state.state = STATE_SENT;
        else
            g_main_loop_quit(loop);

        return;
    break;
    default:
        g_return_if_reached();
    }
}

static gboolean
_ended_close_idle_callback(gpointer user_data)
{
    if ( ! _check_state(-1, 3, STATE_SENT) )
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

static void
_ended_callback(EventcConnection *client, EventdEvent *e, gpointer user_data)
{
    if ( ! _check_state(-1, -1, STATE_SENT) )
        return;

    const gchar *category;
    const gchar *name;

    category = eventd_event_get_category(e);
    name = eventd_event_get_name(e);

    switch ( _test_state.event )
    {
    case 2:
        g_idle_add(_ended_close_idle_callback, client);
    case 1:
    {
        if ( ( g_strcmp0(category, "test") != 0 ) || ( g_strcmp0(name, "answer") != 0 ) )
            break;

        gchar *contents;
        const gchar *filename;
        GError *error = NULL;

        filename = eventd_event_get_data(event, "file");
        if ( ! g_file_get_contents(filename, &contents, NULL, &error) )
        {
            g_warning("Cannot get file contents: %s", error->message);
            g_clear_error(&error);
            break;
        }
        if ( g_strcmp0(eventd_event_get_data(event, "test"), contents) != 0 )
        {
            g_warning("Wrong test file contents: %s", contents);
            g_free(contents);
            break;
        }
        g_free(contents);
        if ( g_unlink(filename) < 0 )
        {
            g_warning("Couldn't remove the file: %s", g_strerror(errno));
            break;
        }

        eventd_event_unref(event);
        _create_event(client);
        if ( ! eventc_connection_event(client, event, &error) )
            g_main_loop_quit(loop);
        else
            _test_state.state = STATE_SENT;
        return;
    }
    case 3:
    default:
        g_return_if_reached();
    }
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

    eventc_connection_set_subscribe(client, TRUE);
    eventc_connection_add_subscription(client, g_strdup("test"));
    g_signal_connect(client, "event", G_CALLBACK(_ended_callback), NULL);

    loop = g_main_loop_new(NULL, FALSE);

    eventc_connection_connect(client, _connect_callback, NULL);

    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    if ( _test_state.connection != MAX_CONNECTION )
    {
        g_warning("Wrong ending state");
        r = 1;
    }

    eventd_event_unref(event);

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
