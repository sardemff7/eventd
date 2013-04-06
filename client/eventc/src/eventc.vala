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
    static string category;
    static string name;
    static string[] event_data_name;
    static string[] event_data_content;
    static string host;
    static GLib.MainLoop loop;
    static Connection client;
    static int tries;
    static int max_tries = 3;
    static int timeout = 0;
    static bool wait = false;

    static bool print_version = false;

    static const GLib.OptionEntry[] entries =
    {
        { "data-name",    'd', 0, GLib.OptionArg.STRING_ARRAY, out event_data_name,    "Event data name to send",                                "<name>" },
        { "data-content", 'c', 0, GLib.OptionArg.STRING_ARRAY, out event_data_content, "Event data content to send (must be after a data-name)", "<content>" },
        { "host",         'h', 0, GLib.OptionArg.STRING,       out host,               "Host to connect to",                                     "<host>" },
        { "max-tries",    'm', 0, GLib.OptionArg.INT,          ref max_tries,          "Maximum connection attempts (0 for infinite)",           "<times>" },
        { "timeout",      'o', 0, GLib.OptionArg.INT,          ref timeout,            "Connection timeout",                                     "<seconds>" },
        { "wait",         'w', 0, GLib.OptionArg.NONE,         ref wait,               "Wait the end of the event",                              null },
        { "version",      'V', 0, GLib.OptionArg.NONE,         ref print_version,      "Print version",                                          null },
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
                GLib.warning("Couldn't connect to host '%s': %s", host, e.message);
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

        var event = new Eventd.Event(category, name);

        for ( uint i = 0 ; i < n_length ; ++i )
            event.add_data(event_data_name[i], event_data_content[i]);

        if ( wait )
        {
            event.ended.connect(() => {
                disconnect();
            });
        }
        client.event.begin(event, (source, res) => {
            try
            {
                client.event.end(res);
            }
            catch ( EventcError e )
            {
                GLib.warning("Couldn't send event '%s': %s", name, e.message);
            }
            if ( ! wait )
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
                GLib.warning("Couldn't disconnect from event: %s", e.message);
            }
            loop.quit();
        });
    }


    public static int
    main(string[] args)
    {
        var opt_context = new GLib.OptionContext("<event category> <event name> - Basic CLI client for eventd");

        opt_context.add_main_entries(entries, Eventd.Config.GETTEXT_PACKAGE);

        try
        {
            opt_context.parse(ref args);
        }
        catch ( OptionError e )
        {
            GLib.error("Couldn't parse the arguments: %s", e.message);
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
            GLib.print("You must define the category of the event.\n");
            return 1;
        }
        if ( args.length < 3 )
        {
            GLib.print("You must define the name of the event.\n");
            return 1;
        }
        category = args[1];
        name = args[2];

        if ( host == null )
            host = "localhost";

        client = new Connection(host);
        client.timeout = timeout;

        tries = 0;
        connect();

        loop = new GLib.MainLoop();

        loop.run();

        return 0;
    }
}
