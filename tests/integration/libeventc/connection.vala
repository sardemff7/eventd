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
connection_test(Eventc.Connection client) throws GLib.Error
{
    yield client.connect();

    var filename = GLib.Path.build_filename(GLib.Environment.get_user_runtime_dir(), "some-other-file");
    var message = "Some other message";

    var event = new Eventd.Event("test");

    event.add_data("file", filename);
    event.add_data("test", message);

    string r = null;
    event.ended.connect((event, reason) => {
        GLib.Idle.add(connection_test.callback);
        if ( reason != Eventd.EventEndReason.RESERVED )
            r = @"Wrong end reason: $reason";
    });

    yield client.event(event);

    GLib.Thread.usleep(100000);

    string contents;
    GLib.FileUtils.get_contents(filename, out contents, null);
    GLib.FileUtils.unlink(filename);
    if ( message != contents )
        return @"Wrong test file contents: $contents";

    // Wait the end
    yield;
    if ( r != null )
        return r;

    yield client.close();

    return null;
}

int
main(string[] args)
{
    int r = 0;
    Eventd.Tests.setup("test-plugin,evp", "9988", "--event-listen", "tcp:localhost:9877", "--no-avahi");
    try
    {
        Eventd.Tests.start_eventd();
    }
    catch ( GLib.Error e )
    {
        GLib.warning("Failed to start eventd: %s", e.message);
        return 99;
    }

    var client = new Eventc.Connection("localhost", 9877, "test");

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
        catch ( Eventc.EventcError e )
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

    try
    {
        Eventd.Tests.stop_eventd();
    }
    catch ( GLib.Error e )
    {
        GLib.warning("Failed to stop eventd: %s", e.message);
        r = 99;
    }
    return r;
}
