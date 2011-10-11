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
    static string event_type;
    static string event_name;
    static string event_data;
    static string host;
    static uint16 port = 0;
    #if ENABLE_GIO_UNIX
    static string unix_socket;
    #endif
    static int max_tries = 3;
    static int timeout = 0;

    static const GLib.OptionEntry[] entries =
    {
        { "type", 't', 0, GLib.OptionArg.STRING, out event_type, N_("Event type to send"), "type" },
        { "name", 'n', 0, GLib.OptionArg.STRING, out event_name, N_("Event name to send"), "name" },
        { "data", 'd', 0, GLib.OptionArg.STRING, out event_data, N_("Event data to send"), "data" },
        { "host", 'h', 0, GLib.OptionArg.STRING, out host, N_("Host to connect to"), "host" },
        { "port", 'p', 0, GLib.OptionArg.INT, ref port, N_("Port to connect to"), "port" },
        #if ENABLE_GIO_UNIX
        { "socket", 's', 0, GLib.OptionArg.FILENAME, out unix_socket, N_("UNIX socket to connect to"), "socket_file" },
        #endif
        { "max-tries", 'm', 0, GLib.OptionArg.INT, ref max_tries, N_("Maximum connection attempts"), "times" },
        { "timeout", 'o', 0, GLib.OptionArg.INT, ref timeout, N_("Connection timeout"), "time" },
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

        if ( args.length < 2 )
        {
            GLib.print(_("You must define the type of the client.\n"));
            return 1;
        }
        type = args[1];

        if ( args.length > 2 )
            name = string.joinv(" ", args[2:args.length-1]);
        else
            name = null;

        if ( host == null )
            host = "localhost";

        var client = new Eventc(host, port, type, name);
        client.timeout = timeout;

        var tries = 0;
        while ( ! client.is_connected() )
        {
            try
            {
                client.connect();
            }
            catch ( EventcError e )
            {
                GLib.warning("Couldn’t connect to host '%s': %s", host, e.message);
                if ( ++tries >= max_tries )
                {
                    GLib.warning("Too many attempts, aborting");
                    return 1;
                }
            }
        }

        if ( event_type != null )
        {
            try
            {
                client.event(event_type, event_name, event_data);
            }
            catch ( EventcError e )
            {
                GLib.warning("Couldn’t send event '%s' / '%s': %s", event_type, event_name, e.message);
            }
        }
        else
        {
            string line = null;
            while ( ( line = stdin.read_line() ) != null )
            {
                var elements = line.split(" ", 2);
                event_type = elements[0];
                event_name = elements[1];
                try
                {
                    client.event(event_type, event_name, null);
                }
                catch ( EventcError e )
                {
                    GLib.warning("Couldn’t send event %s / %s: %s", event_type, event_name, e.message);
                }
            }
        }

        try
        {
            client.close();
        }
        catch ( EventcError e )
        {
            GLib.warning("Couldn’t close connection to host '%s': %s", host, e.message);
            return 1;
        }

        return 0;
    }
}
