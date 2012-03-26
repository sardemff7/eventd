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

int
connection_test(GLib.DataInputStream input, GLib.DataOutputStream output) throws GLib.Error
{
    string r;

    output.put_string("HELLO test\n");
    r = input.read_upto("\n", -1, null);
    input.read_byte(null);
    if ( r != "HELLO" )
        return 1;

    output.put_string("MODE normal\n");
    r = input.read_upto("\n", -1, null);
    input.read_byte(null);
    if ( r != "MODE" )
        return 1;

    var filename = GLib.Path.build_filename(GLib.Environment.get_user_runtime_dir(), "some-file");
    var message = "Some message";

    output.put_string("EVENT test\n");
    output.put_string(@"DATAL file $filename\n");
    output.put_string(@"DATAL test $message\n");
    output.put_string(".\n");
    r = input.read_upto("\n", -1, null);
    input.read_byte(null);
    if ( r != "OK" )
        return 1;

    GLib.Thread.usleep(100000);

    string contents;
    GLib.FileUtils.get_contents(filename, out contents, null);
    GLib.FileUtils.unlink(filename);
    if ( message != contents )
        return 1;

    output.put_string("BYE\n");
    r = input.read_upto("\n", -1, null);
    input.read_byte(null);
    if ( r != "BYE" )
        return 1;

    return 0;
}

int
main(string[] args)
{
    int r = 0;
    GLib.Pid pid;
    try
    {
        Eventd.Tests.start_eventd(out pid, "test-plugin,evp", "--event-listen=tcp:localhost:9876");
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

        r = connection_test(input, output);

        connection.close();
    }
    catch ( GLib.Error e )
    {
        GLib.warning("Test failed: %s", e.message);
        r = 99;
    }

    try
    {
        Eventd.Tests.stop_eventd(pid);
    }
    catch ( GLib.Error e )
    {
        GLib.warning("Failed to stop eventd: %s", e.message);
        r = 99;
    }
    return r;
}
