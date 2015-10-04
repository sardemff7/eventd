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

static GError *error = NULL;
static EventdEvent *event = NULL;
static GMainLoop *loop = NULL;

static enum {
    STATE_FIRST_CONNECTION_FIRST_EVENT = 1,
    STATE_FIRST_CONNECTION_SECOND_EVENT = 2,
    STATE_SECOND_CONNECTION_FIRST_EVENT = 3,
    STATE_SECOND_CONNECTION_SECOND_EVENT = 4,
    STATE_END = 5
} state = STATE_FIRST_CONNECTION_FIRST_EVENT;

static void _ended_callback(EventdEvent *e, EventdEventEndReason reason, EventcConnection *client);
static void _answered_callback(EventdEvent *e, const gchar *answer, EventcConnection *client);
static void
_create_event(EventcConnection *client, gboolean with_file)
{
    event = eventd_event_new("test", "test");
    if ( with_file )
    {
        gchar *tmp = g_strconcat(g_get_prgname(), "-file", NULL );
        eventd_event_add_data(event, g_strdup("file"), g_build_filename(g_getenv("EVENTD_TESTS_TMP_DIR"), tmp, NULL));
        g_free(tmp);
        eventd_event_add_data(event, g_strdup("test"), g_strdup_printf("Some message from %s", g_get_prgname()));
        eventd_event_add_answer(event,"test");
    }

    g_signal_connect(event, "answered", G_CALLBACK(_answered_callback), client);
    g_signal_connect(event, "ended", G_CALLBACK(_ended_callback), client);
}

static void
_connect_callback(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    EventcConnection *client = EVENTC_CONNECTION(obj);

    if ( eventc_connection_connect_finish(client, res, &error) )
    {
        switch ( state )
        {
        case STATE_SECOND_CONNECTION_FIRST_EVENT:
            g_object_unref(event);
            _create_event(client, TRUE);
        case STATE_FIRST_CONNECTION_FIRST_EVENT:
            if ( ! eventc_connection_event(client, event, &error) )
                break;
            return;
        break;
        default:
            g_warning("Should never be in that state");
        }
    }
    g_main_loop_quit(loop);
}

static void
_answered_callback(EventdEvent *e, const gchar *answer, EventcConnection *client)
{
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

    return;
fail:
    g_main_loop_quit(loop);
}

static gboolean
_ended_close_idle_callback(gpointer user_data)
{
    EventcConnection *client = user_data;

    switch ( state )
    {
    case STATE_FIRST_CONNECTION_SECOND_EVENT:
        if ( ! eventc_connection_close(client, &error) )
            break;
        eventc_connection_connect(client, _connect_callback, NULL);
        state = STATE_SECOND_CONNECTION_FIRST_EVENT;
        return FALSE;
    case STATE_SECOND_CONNECTION_SECOND_EVENT:
        eventc_connection_close(client, &error);
        state = STATE_END;
    break;
    default:
        g_warning("Should never be in that state");
    }
    g_main_loop_quit(loop);
    return FALSE;
}

static gboolean
_end_idle_callback(gpointer user_data)
{
    switch ( state )
    {
    case STATE_FIRST_CONNECTION_SECOND_EVENT:
    case STATE_SECOND_CONNECTION_SECOND_EVENT:
        eventd_event_end(event, EVENTD_EVENT_END_REASON_CLIENT_DISMISS);
    break;
    default:
        g_warning("Should never be in that state");
        g_main_loop_quit(loop);
    }
    return FALSE;
}

static void
_ended_callback(EventdEvent *e, EventdEventEndReason reason, EventcConnection *client)
{
    g_return_if_fail(eventd_event_end_reason_is_valid_value(reason));
    switch ( state )
    {
    case STATE_FIRST_CONNECTION_FIRST_EVENT:
    case STATE_SECOND_CONNECTION_FIRST_EVENT:
        if ( reason != EVENTD_EVENT_END_REASON_TEST )
            break;
        g_object_unref(event);
        _create_event(client, FALSE);
        if ( ! eventc_connection_event(client, event, &error) )
            g_main_loop_quit(loop);
        else
            g_idle_add(_end_idle_callback, client);
        ++state;
        return;
    case STATE_FIRST_CONNECTION_SECOND_EVENT:
    case STATE_SECOND_CONNECTION_SECOND_EVENT:
        if ( reason != EVENTD_EVENT_END_REASON_CLIENT_DISMISS )
            break;
        g_idle_add(_ended_close_idle_callback, client);
        return;
    default:
        g_warning("Should never be in that state");
    }
    g_warning("Wrong end reason: %s", eventd_event_end_reason_get_value_nick(reason));
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

    _create_event(client, TRUE);

    loop = g_main_loop_new(NULL, FALSE);

    eventc_connection_connect(client, _connect_callback, NULL);

    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    if ( state != STATE_END )
        r = 1;

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
