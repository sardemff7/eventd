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

string?
connection_test(GLib.DataInputStream input, GLib.DataOutputStream output) throws GLib.Error
{
    string r;

    var filename = GLib.Path.build_filename(GLib.Environment.get_variable("EVENTD_TESTS_TMP_DIR"), "evp-connection-file");
    var message = "Some message";

    output.put_string("EVENT 1 test test\n");
    output.put_string("ANSWER test\n");
    output.put_string(@"DATAL file $filename\n");
    output.put_string(@"DATA test\n$message\n.\n");
    output.put_string(".\n");
    r = input.read_upto("\n", -1, null);
    input.read_byte(null);
    if ( r != "EVENT 1" )
        return @"Bad answer to EVENT: $r";

    r = input.read_upto("\n", -1, null);
    input.read_byte(null);
    if ( r != "ANSWERED 1 test" )
        return @"Wrong ANSWER to EVENT 1: $r";

    r = input.read_upto("\n", -1, null);
    input.read_byte(null);
    if ( r != @"DATAL test $message" )
        return @"Wrong ANSWER DATAL to EVENT 1: $r";

    r = input.read_upto("\n", -1, null);
    input.read_byte(null);
    if ( r != "." )
        return @"Wrong ANSWER end to EVENT 1: $r";

    r = input.read_upto("\n", -1, null);
    input.read_byte(null);
    if ( r != "ENDED 1 reserved" )
        return @"No ENDED or bad ENDED to EVENT 1: $r";

    string contents;
    GLib.FileUtils.get_contents(filename, out contents, null);
    GLib.FileUtils.unlink(filename);
    if ( message != contents )
        return @"Wrong test file contents: $contents";

    output.put_string("EVENT 2 test test\n");
    output.put_string(".\n");
    r = input.read_upto("\n", -1, null);
    input.read_byte(null);
    if ( r != "EVENT 2" )
        return @"Bad answer to EVENT: $r";

    output.put_string("END 2\n");
    r = input.read_upto("\n", -1, null);
    input.read_byte(null);
    if ( r != "ENDING 2" )
        return @"Bad answer to END: $r";

    r = input.read_upto("\n", -1, null);
    input.read_byte(null);
    if ( r != "ENDED 2 client-dismiss" )
        return @"No ENDED or bad ENDED to EVENT 2: $r";

    output.put_string("BYE\n");

    return null;
}

string?
connection_fail_test(GLib.DataInputStream input, GLib.DataOutputStream output) throws GLib.Error
{
    string r;

    output.put_string("some crap\n");
    r = input.read_upto("\n", -1, null);
    input.read_byte(null);
    if ( r != "ERROR unknown" )
        return @"Not detecting unknown message: $r";

    output.put_string("EVENT 1 test test\n");
    output.put_string("some crap\n");
    output.put_string(".\n");
    r = input.read_upto("\n", -1, null);
    input.read_byte(null);
    if ( r != "ERROR bad-event" )
        return @"Bad answer to bad EVENT: $r";

    output.put_string("END 1\n");
    r = input.read_upto("\n", -1, null);
    input.read_byte(null);
    if ( r != "ERROR bad-id" )
        return @"Bad answer to bad END: $r";


    output.put_string("BYE\n");

    return null;
}

int
main(string[] args)
{
    int r = 0;
    Eventd.Tests.Env.setup();
    var env = new Eventd.Tests.Env("test-plugin,evp", "9987", { "--event-listen", "tcp:localhost4:9876", "--no-avahi" });
    try
    {
        env.start_eventd();
    }
    catch ( GLib.Error e )
    {
        GLib.warning("Failed to start eventd: %s", e.message);
        return 99;
    }

    var client = new GLib.SocketClient();
    var address = new GLib.InetSocketAddress(new GLib.InetAddress.loopback(GLib.SocketFamily.IPV4), 9876);
    GLib.SocketConnection connection;
    GLib.DataInputStream input;
    GLib.DataOutputStream output;
    string m;

    try
    {
        connection = client.connect(address, null);

        input = new GLib.DataInputStream((connection as GLib.IOStream).get_input_stream());
        output = new GLib.DataOutputStream((connection as GLib.IOStream).get_output_stream());

        m = connection_test(input, output);
        if ( m != null )
        {
            r = 1;
            GLib.warning("Test failed: %s", m);
        }

        connection.close();
    }
    catch ( GLib.Error e )
    {
        GLib.warning("Test failed: %s", e.message);
        r = 99;
    }

    try
    {
        connection = client.connect(address, null);

        input = new GLib.DataInputStream((connection as GLib.IOStream).get_input_stream());
        output = new GLib.DataOutputStream((connection as GLib.IOStream).get_output_stream());

        m = connection_fail_test(input, output);
        if ( m != null )
        {
            r = 1;
            GLib.warning("Test failed: %s", m);
        }

        connection.close();
    }
    catch ( GLib.Error e )
    {
        GLib.warning("Test failed: %s", e.message);
        r = 99;
    }

    try
    {
        env.stop_eventd();
    }
    catch ( GLib.Error e )
    {
        GLib.warning("Failed to stop eventd: %s", e.message);
        r = 99;
    }
    return r;
}
