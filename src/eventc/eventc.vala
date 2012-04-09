/*
 * eventc - Basic CLI client for eventd
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

namespace Eventc
{
    static string type;
    static string event_name;
    static string[] event_data_name;
    static string[] event_data_content;
    static string host;
    static uint16 port = 0;
    #if HAVE_GIO_UNIX
    static string unix_socket;
    #endif
    static GLib.MainLoop loop;
    static Connection client;
    static int tries;
    static int max_tries = 3;
    static int timeout = 0;

    static bool print_version = false;

    static const GLib.OptionEntry[] entries =
    {
        { "name",         'n', 0, GLib.OptionArg.STRING,       out event_name,         N_("Event name to send"),                                     "<name>" },
        { "data-name",    'd', 0, GLib.OptionArg.STRING_ARRAY, out event_data_name,    N_("Event data name to send"),                                "<name>" },
        { "data-content", 'c', 0, GLib.OptionArg.STRING_ARRAY, out event_data_content, N_("Event data content to send (must be after a data-name)"), "<content>" },
        { "host",         'h', 0, GLib.OptionArg.STRING,       out host,               N_("Host to connect to"),                                     "<host>" },
        { "port",         'p', 0, GLib.OptionArg.INT,          ref port,               N_("Port to connect to"),                                     "<port>" },
        #if HAVE_GIO_UNIX
        { "socket",       's', 0, GLib.OptionArg.FILENAME,     out unix_socket,        N_("UNIX socket to connect to"),                              "<socket file>" },
        #endif
        { "max-tries",    'm', 0, GLib.OptionArg.INT,          ref max_tries,          N_("Maximum connection attempts (0 for infinite)"),           "<times>" },
        { "timeout",      'o', 0, GLib.OptionArg.INT,          ref timeout,            N_("Connection timeout"),                                     "<seconds>" },
        { "version",      'V', 0, GLib.OptionArg.NONE,         ref print_version,      N_("Print version"),                                          null },
        { null }
    };

    private new static void
    connect()
    {
        client.connect.begin((source, res) => {
            try
            {
                client.connect.end(res);
                send_event();
            }
            catch ( EventcError e )
            {
                GLib.warning("Couldn’t connect to host '%s': %s", host, e.message);
                if ( ( max_tries > 0 ) && ( ++tries >= max_tries ) )
                {
                    GLib.warning("Too many attempts, aborting");
                    loop.quit();
                }
                else
                    connect();
            }
        });
    }

    private static void
    send_event()
    {
        var n_length = ( event_data_name == null ) ? 0 : strv_length(event_data_name);
        var c_length = ( event_data_content == null ) ? 0 : strv_length(event_data_content);
        if ( n_length != c_length )
        {
            GLib.warning("Not the same number of data names and data contents");
                loop.quit();
            return;
        }

        var event = new Eventd.Event(event_name);

        for ( uint i = 0 ; i < n_length ; ++i )
            event.add_data(event_data_name[i], event_data_content[i]);

        client.event.begin(event, (source, res) => {
            try
            {
                client.event.end(res);
            }
            catch ( EventcError e )
            {
                GLib.warning("Couldn’t send event '%s': %s", event_name, e.message);
            }
            disconnect();
        });
    }

    private static void
    disconnect()
    {
        client.close.begin((source, res) => {
            try
            {
                client.close.end(res);
            }
            catch ( EventcError e )
            {
                GLib.warning("Couldn’t disconnect from event: %s", e.message);
            }
            loop.quit();
        });
    }


    public static int
    main(string[] args)
    {
        var opt_context = new GLib.OptionContext("<client type> - Basic CLI client for eventd");

        opt_context.add_main_entries(entries, Eventd.Config.GETTEXT_PACKAGE);

        try
        {
            opt_context.parse(ref args);
        }
        catch ( OptionError e )
        {
            GLib.error("Couldn’t parse the arguments: %s", e.message);
        }

        if ( print_version )
        {
            stdout.printf("eventc %s (using libeventc %s)\n",
                Eventd.Config.PACKAGE_VERSION,
                Eventc.get_version());

            return 0;
        }

        if ( args.length < 2 )
        {
            GLib.print(_("You must define the type of the client.\n"));
            return 1;
        }
        if ( event_name == null )
        {
            GLib.print(_("You must define the name of the event.\n"));
            return 1;
        }
        type = args[1];

        #if HAVE_GIO_UNIX
        if ( unix_socket != null )
            host = unix_socket;
        #endif
        if ( host == null )
            host = "localhost";

        client = new Connection(host, port, type);
        client.timeout = timeout;
        #if HAVE_GIO_UNIX
        if ( unix_socket != null )
            client.host_is_socket = true;
        #endif

        tries = 0;
        connect();

        loop = new GLib.MainLoop();

        loop.run();

        return 0;
    }
}
