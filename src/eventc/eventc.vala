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

namespace Eventc
{
    static string type;
    static string mode;
    static string name;
    static string event_type;
    static string[] event_data_name;
    static string[] event_data_content;
    static string host;
    static uint16 port = 0;
    #if ENABLE_GIO_UNIX
    static string unix_socket;
    #endif
    static int max_tries = 3;
    static int timeout = 0;

    static const GLib.OptionEntry[] entries =
    {
        { "type", 't', 0, GLib.OptionArg.STRING, out event_type, N_("Event type to send"), "<type>" },
        { "mode", 0, 0, GLib.OptionArg.STRING, out mode, N_("Mode of the client"), "{normal|ping-pong}" },
        { "data-name", 'n', 0, GLib.OptionArg.STRING_ARRAY, out event_data_name, N_("Event data name to send"), "<name>" },
        { "data-content", 'c', 0, GLib.OptionArg.STRING_ARRAY, out event_data_content, N_("Event data content to send (must be after a data-name)"), "<content>" },
        { "host", 'h', 0, GLib.OptionArg.STRING, out host, N_("Host to connect to"), "<host>" },
        { "port", 'p', 0, GLib.OptionArg.INT, ref port, N_("Port to connect to"), "<port>" },
        #if ENABLE_GIO_UNIX
        { "socket", 's', 0, GLib.OptionArg.FILENAME, out unix_socket, N_("UNIX socket to connect to"), "<socket file>" },
        #endif
        { "max-tries", 'm', 0, GLib.OptionArg.INT, ref max_tries, N_("Maximum connection attempts (0 for infinite)"), "<times>" },
        { "timeout", 'o', 0, GLib.OptionArg.INT, ref timeout, N_("Connection timeout"), "<seconds>" },
        { null }
    };

    private static void print_pong_data(Eventd.Event event)
    {
        unowned GLib.HashTable<string, string>? ret_data = event.get_pong_data();
        if ( ret_data == null )
            return;

        ret_data.foreach((name, content) => {
            if ( content.index_of_char('\n') == -1 )
                stdout.puts(@"DATAL $name $content\n");
            else
            {
                stdout.puts(@"DATA $name\n");
                var datas = content.split("\n");
                foreach ( var line in datas )
                {
                    if ( line[0] == '.' )
                        stdout.puts(".");
                    stdout.puts(line);
                    stdout.puts("\n");
                }
                stdout.puts(".\n");
            }
        });
        stdout.puts(".\n");
    }

    public static int main(string[] args)
    {
        var opt_context = new GLib.OptionContext("<client type> [<client name>] - Basic CLI client for eventd");

        opt_context.add_main_entries(entries, Eventd.Config.GETTEXT_PACKAGE);

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

        #if ENABLE_GIO_UNIX
        if ( unix_socket != null )
            host = unix_socket;
        #endif
        if ( host == null )
            host = "localhost";

        var client = new Connection(host, port, type, name);
        client.timeout = timeout;
        #if ENABLE_GIO_UNIX
        if ( unix_socket != null )
            client.host_is_socket = true;
        #endif

        switch ( mode )
        {
        case "ping-pong":
            client.mode = Connection.Mode.PING_PONG;
        break;
        case "normal":
        default:
            client.mode = Connection.Mode.NORMAL;
        break;
        }

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
                if ( ( max_tries > 0 ) && ( ++tries >= max_tries ) )
                {
                    GLib.warning("Too many attempts, aborting");
                    return 1;
                }
            }
        }

        if ( event_type != null )
        {
            var n_length = ( event_data_name == null ) ? 0 : strv_length(event_data_name);
            var c_length = ( event_data_content == null ) ? 0 : strv_length(event_data_content);
            if ( n_length != c_length )
            {
                GLib.warning("Not the same number of data names and data contents");
                return 1;
            }

            var event = new Eventd.Event(event_type);

            for ( uint i = 0 ; i < n_length ; ++i )
                event.add_data(event_data_name[i], event_data_content[i]);

            try
            {
                client.event(event);
            }
            catch ( EventcError e )
            {
                GLib.warning("Couldn’t send event '%s': %s", event_type, e.message);
            }
            print_pong_data(event);
        }
        else
        {
            while ( ( event_type = stdin.read_line() ) != null )
            {
                var event = new Eventd.Event(event_type);
                try
                {
                    client.event(event);
                }
                catch ( EventcError e )
                {
                    GLib.warning("Couldn’t send event '%s': %s", event_type, e.message);
                }
                print_pong_data(event);
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
