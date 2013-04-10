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
        NOT_CONNECTED,
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
            error = Connection.error,

            answered = Connection.answered,
            ended = Connection.ended,
            bye = Connection.bye
        };

        public string host { set; private get; }

        public bool enable_proxy { get; set; default = true; }

        private Libeventd.Evp.Context evp;

        private uint64 count;
        private GLib.HashTable<string, Eventd.Event> events;
        private GLib.HashTable<Eventd.Event, string> ids;

        public
        Connection(string host)
        {
            this.host = host;

            this.evp = new Libeventd.Evp.Context((void *)this, ref Connection.client_interface);
            this.events = new GLib.HashTable<string, Eventd.Event>(GLib.str_hash, GLib.str_equal);
            this.ids = new GLib.HashTable<Eventd.Event, string>(GLib.direct_hash, GLib.direct_equal);
        }

        public bool
        is_connected() throws EventcError
        {
            try
            {
                return ( this.evp.is_connected() );
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

            GLib.SocketConnectable address;
            try
            {
                address = Libeventd.Evp.get_address(this.host);
            }
            catch ( GLib.Error re )
            {
                throw new EventcError.HOSTNAME("Couldn't resolve the hostname '%s': %s", this.host, re.message);
            }

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
        }

        public void
        event(Eventd.Event event) throws EventcError
        {
            if ( ! this.is_connected() )
                throw new EventcError.NOT_CONNECTED("not connected, you must connect first");

            string id = (++this.count).to_string("%" + int64.FORMAT_MODIFIER + "x");
            try
            {
                this.evp.send_event(id, event);
            }
            catch ( GLib.Error e )
            {
                this.close_internal();
                throw new EventcError.EVENT("Couldn't send event: %s", e.message);
            }
            this.events.insert(id, event);
            this.ids.insert(event, id);
            event.ended.connect(() => {
                id = this.ids.lookup(event);
                this.events.remove(id);
                this.ids.remove(event);
            });
        }

        public void
        event_end(Eventd.Event event) throws EventcError
        {
            if ( ! this.is_connected() )
                throw new EventcError.NOT_CONNECTED("not connected, you must connect first");

            unowned string id;
            id = this.ids.lookup(event);
            if ( id == null )
                return;
            try
            {
                this.evp.send_end(id);
            }
            catch ( GLib.Error e )
            {
                this.close_internal();
                throw new EventcError.END("Couldn't send event end: %s", e.message);
            }
        }

        public async void
        close() throws EventcError
        {
            if ( this.is_connected() )
                this.evp.send_bye();
            yield this.close_internal();
        }

        private void
        error(Libeventd.Evp.Context context, GLib.Error error)
        {
            GLib.warning("Error: %s", error.message);
        }

        private void
        answered(Libeventd.Evp.Context context, string id, string answer, GLib.HashTable<string, string> data_hash)
        {
            var event = this.events.lookup(id);
            if ( event == null )
                return;

            Eventd.PrivateEvent.set_all_answer_data(event, data_hash);
            event.answer(answer);
        }

        private void
        ended(Libeventd.Evp.Context context, string id, Eventd.EventEndReason reason)
        {
            var event = this.events.lookup(id);
            if ( event == null )
                return;

            event.end(reason);
        }

        private void
        bye(Libeventd.Evp.Context context)
        {
            this.close_internal();
        }

        private async void
        close_internal()
        {
            this.events.remove_all();
            yield this.evp.close();
        }
    }
}
