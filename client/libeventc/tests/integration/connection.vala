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

async string?
connection_test_internal(Eventc.Connection client) throws GLib.Error
{
    yield client.connect();

    var filename = GLib.Path.build_filename(GLib.Environment.get_variable("EVENTD_TESTS_TMP_DIR"), "libeventc-connection-file");
    var message = "Some other message";

    var event = new Eventd.Event("test", "test");

    event.add_data("file", filename);
    event.add_data("test", message);
    event.add_answer("test");

    string r = null;
    string contents = message;
    GLib.Error e = null;
    event.answered.connect((event, answer) => {
        GLib.Idle.add(connection_test_internal.callback);
        if ( answer != "test" )
        {
            r = @"Wrond answer to event: $answer";
            return;
        }
        try
        {
            GLib.FileUtils.get_contents(filename, out contents, null);
            GLib.FileUtils.unlink(filename);
        }
        catch ( GLib.Error ie )
        {
            e = ie;
        }
    });

    event.ended.connect((event, reason) => {
        GLib.Idle.add(connection_test_internal.callback);
        if ( reason != Eventd.EventEndReason.RESERVED )
            r = @"Wrong end reason: $reason";
    });

    client.event(event);

    // Wait the answer
    yield;
    if ( e != null )
        throw e;
    if ( r != null )
        return r;

    if ( message != contents )
        return @"Wrong test file contents: $contents";

    // Wait the end
    yield;
    if ( r != null )
        return r;


    event = new Eventd.Event("test", "test");

    event.ended.connect((event, reason) => {
        GLib.Idle.add(connection_test_internal.callback);
        if ( reason != Eventd.EventEndReason.CLIENT_DISMISS )
            r = @"Wrong end reason: $reason";
    });

    client.event(event);

    client.event_end(event);

    // Wait the end
    yield;
    if ( r != null )
        return r;


    yield client.close();

    return null;
}

async string?
connection_test(Eventc.Connection client) throws GLib.Error
{
    var r = yield connection_test_internal(client);
    if ( r != null )
        return r;
    return yield connection_test_internal(client);
}

int
main(string[] args)
{
    int r = 0;
    Eventd.Tests.Env.setup();
    var env = new Eventd.Tests.Env("test-plugin,evp", "18021", { "--event-listen", "tcp:localhost4:19021", "--no-avahi" });
    if ( ! env.start_eventd() )
        return 99;

    var client = new Eventc.Connection("127.0.0.1:19021");

    var loop = new GLib.MainLoop(null);

    string error = null;

    connection_test.begin(client, (obj, res) => {
        try
        {
            error = connection_test.end(res);
            if ( error != null )
            {
                r = 1;
                GLib.warning("Test failed: %s", error);
            }
        }
        catch ( Eventc.Error e )
        {
            r = 2;
            GLib.warning("Test failed: %s", e.message);
        }
        catch ( GLib.Error e )
        {
            r = 99;
            GLib.warning("Test failed: %s", e.message);
        }
        loop.quit();
    });

    loop.run();

    if ( ! env.stop_eventd() )
        return 99;
    return r;
}
