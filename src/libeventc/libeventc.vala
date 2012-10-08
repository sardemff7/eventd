/*
 * libeventc - Library to communicate with eventd
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
    public errordomain EventcError
    {
        HOSTNAME,
        CONNECTION_REFUSED,
        CONNECTION_OTHER,
        ALREADY_CONNECTED,
        HELLO,
        RECEIVE,
        EVENT,
        END,
        BYE
    }

    public static unowned string
    get_version()
    {
        return Eventd.Config.PACKAGE_VERSION;
    }

    public class Connection : GLib.Object
    {
        static Libeventd.Evp.ClientInterface client_interface = Libeventd.Evp.ClientInterface() {
            get_event = Connection.get_event,
            error = Connection.error,

            answered = Connection.answered,
            ended = Connection.ended,
            bye = Connection.bye
        };

        public string host { set; private get; }
        public uint16 port { set; private get; }

        private string category;

        public uint timeout { get; set; default = 0; }
        public bool enable_proxy { get; set; default = true; }

        private Libeventd.Evp.Context evp;
        private GLib.HashTable<string, Eventd.Event> events;

        private bool handshake_passed;

        public
        Connection(string host, uint16 port, string category)
        {
            this.host = host;
            this.port = port;
            this.category = category;

            this.evp = new Libeventd.Evp.Context((void *)this, ref Connection.client_interface);
            this.events = new GLib.HashTable<string, Eventd.Event>(GLib.str_hash, GLib.str_equal);
        }

        public bool
        is_connected() throws EventcError
        {
            try
            {
                return ( this.evp.is_connected() && this.handshake_passed );
            }
            catch ( GLib.Error e )
            {
                throw new EventcError.RECEIVE("Failed to receive message: %s", e.message);
            }
        }

        public new async void
        connect() throws EventcError
        {
            if ( this.is_connected() )
                throw new EventcError.ALREADY_CONNECTED("Already connected, you must disconnect first");

            this.handshake_passed = false;
            var address = Libeventd.Evp.get_address(this.host, this.port);
            if ( address == null )
                throw new EventcError.HOSTNAME("Couldn't resolve the hostname");

            var client = new GLib.SocketClient();
            client.set_enable_proxy(this.enable_proxy);
            GLib.SocketConnection connection;
            try
            {
                connection = yield client.connect_async(address);
            }
            catch ( GLib.IOError.CONNECTION_REFUSED ie )
            {
                throw new EventcError.CONNECTION_REFUSED("Host is not an eventd");
            }
            catch ( GLib.Error e )
            {
                throw new EventcError.CONNECTION_OTHER("Failed to connect: %s", e.message);
            }
            this.evp.set_connection(connection);

            this.evp.receive_loop_client();

            yield this.hello();
        }

        private async void
        hello() throws EventcError
        {
            try
            {
                yield this.evp.send_hello(this.category);
            }
            catch ( GLib.Error e )
            {
                throw new EventcError.HELLO(e.message);
            }
            this.handshake_passed = true;
        }

        public async void
        event(Eventd.Event event) throws EventcError
        {
            unowned string id;
            try
            {
                id = yield this.evp.send_event(event);
            }
            catch ( GLib.Error e )
            {
                this.handshake_passed = false;
                throw new EventcError.EVENT("Couldn't send event: %s", e.message);
            }
            event.set_id(id);
            this.events.insert(id, event);
        }

        public async void
        event_end(Eventd.Event event) throws EventcError
        {
            try
            {
                yield this.evp.send_end(event.get_id());
            }
            catch ( GLib.Error e )
            {
                this.handshake_passed = false;
                throw new EventcError.END("Couldn't send event end: %s", e.message);
            }
        }

        public async void
        close() throws EventcError
        {
            this.events.remove_all();

            if ( this.is_connected() )
                this.evp.send_bye();
            yield this.evp.close();
            this.handshake_passed = false;
        }

        private unowned Eventd.Event?
        get_event(Libeventd.Evp.Context context, string id)
        {
            return this.events.lookup(id);
        }

        private void
        error(Libeventd.Evp.Context context, GLib.Error error)
        {
            GLib.warning("Error: %s", error.message);
        }

        private void
        answered(Libeventd.Evp.Context context, Eventd.Event event, string answer)
        {
            event.answer(answer);
        }

        private void
        ended(Libeventd.Evp.Context context, Eventd.Event event, Eventd.EventEndReason reason)
        {
            event.end(reason);
        }

        private void
        bye(Libeventd.Evp.Context context)
        {
            this.handshake_passed = false;
        }
    }
}
