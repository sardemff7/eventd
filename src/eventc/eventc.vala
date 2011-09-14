/*
 * eventc - Basic CLI client for eventd
 *
 * Copyright © 2011 Quentin "Sardem FF7" Glidic
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

namespace Eventd
{
    static string type;
    static string name;
    static string event;
    static string data;
    static string host;
    static uint16 port = 0;
    #if ENABLE_GIO_UNIX
    static string unix_socket;
    #endif

    static const GLib.OptionEntry[] entries =
    {
        { "type", 't', 0, GLib.OptionArg.STRING, out type, N_("Type of the client"), "type" },
        { "name", 'n', 0, GLib.OptionArg.STRING, out name, N_("Name of the client"), "name" },
        { "event", 'e', 0, GLib.OptionArg.STRING, out event, N_("Event to send"), "event" },
        { "data", 'd', 0, GLib.OptionArg.STRING, out data, N_("Data to send"), "data" },
        { "host", 'h', 0, GLib.OptionArg.STRING, out host, N_("Host to connect to"), "host" },
        { "port", 'p', 0, GLib.OptionArg.INT, ref port, N_("Port to connect to"), "port" },
        #if ENABLE_GIO_UNIX
        { "socket", 's', 0, GLib.OptionArg.FILENAME, out unix_socket, N_("UNIX socket to connect to"), "socket_file" },
        #endif
        { null }
    };

    public static int main(string[] args)
    {
        var opt_context = new GLib.OptionContext("- Basic CLI client for eventd");

        opt_context.add_main_entries(entries, Config.GETTEXT_PACKAGE);

        try
        {
            opt_context.parse(ref args);
        }
        catch ( OptionError e )
        {
            GLib.error("Couldn’t parse the arguments: %s", e.message);
        }

        if ( type == null )
        {
            GLib.print(_("You must define the type of the client.\n"));
            return 1;
        }

        if ( host == null )
            host = "localhost";

        var client = new Eventc(host, port, type, name);

        client.connect();
        if ( event != null )
        {
            client.event(event, null, data);
        }
        else
        {
            string line = null;
            while ( ( line = stdin.read_line() ) != null )
            {
                var elements = line.split(" ", 2);
                event = elements[0];
                data = elements[1];
                client.event(event, null, data);
            }
        }
        client.close();

        return 0;
    }
}
