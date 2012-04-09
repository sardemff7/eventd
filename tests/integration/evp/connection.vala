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

    output.put_string("HELLO test\n");
    r = input.read_upto("\n", -1, null);
    input.read_byte(null);
    if ( r != "HELLO" )
        return @"Bad handshake: $r";

    var filename = GLib.Path.build_filename(GLib.Environment.get_user_runtime_dir(), "some-file");
    var message = "Some message";

    output.put_string("EVENT test\n");
    output.put_string(@"DATAL file $filename\n");
    output.put_string(@"DATA test\n$message\n.\n");
    output.put_string(".\n");
    r = input.read_upto("\n", -1, null);
    input.read_byte(null);
    if ( r != "OK" )
        return @"Bad answer to EVENT: $r";

    GLib.Thread.usleep(100000);

    string contents;
    GLib.FileUtils.get_contents(filename, out contents, null);
    GLib.FileUtils.unlink(filename);
    if ( message != contents )
        return @"Wrong test file contents: $contents";

    output.put_string("BYE\n");

    return null;
}

int
main(string[] args)
{
    int r = 0;
    try
    {
        Eventd.Tests.start_eventd("test-plugin,evp", "--event-listen=tcp:localhost:9876", "--no-avahi");
    }
    catch ( GLib.Error e )
    {
        GLib.warning("Failed to start eventd: %s", e.message);
        return 99;
    }

    try
    {
        var client = new GLib.SocketClient();
        var address = new GLib.InetSocketAddress(new GLib.InetAddress.loopback(GLib.SocketFamily.IPV4), 9876);
        var connection = client.connect(address, null);

        var input = new GLib.DataInputStream((connection as GLib.IOStream).get_input_stream());
        var output = new GLib.DataOutputStream((connection as GLib.IOStream).get_output_stream());

        var m = connection_test(input, output);
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
        Eventd.Tests.stop_eventd();
    }
    catch ( GLib.Error e )
    {
        GLib.warning("Failed to stop eventd: %s", e.message);
        r = 99;
    }
    return r;
}
