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
        MODE,
        BYE,
        RENAMED,
        SEND,
        RECEIVE,
        EVENT,
        CLOSE
    }

    public static unowned string
    get_version()
    {
        return Eventd.Config.PACKAGE_VERSION;
    }

    public class Connection : GLib.Object
    {
        private GLib.Mutex mutex;

        private string _host;
        private uint16 _port;

        public string host
        {
            set
            {
                if ( this._host != value )
                {
                    this._host = value;
                    this.address = null;
                }
            }
        }

        public uint16 port
        {
            set
            {
                if ( this._port != value )
                {
                    this._port = value;
                    this.address = null;
                }
            }
        }

        private string category;

        #if HAVE_GIO_UNIX
        public bool host_is_socket { get; set; default = false; }
        #endif

        public uint timeout { get; set; default = 0; }
        public bool enable_proxy { get; set; default = true; }

        private GLib.SocketConnectable address;
        private GLib.SocketClient client;
        private GLib.SocketConnection connection;
        private GLib.DataInputStream input;
        private GLib.DataOutputStream output;

        private bool handshake_passed;

        public
        Connection(string host, uint16 port, string category)
        {
            this.mutex = new GLib.Mutex();
            this._host = host;
            this._port = port;
            this.category = category;

            this.client = new GLib.SocketClient();
        }

        public bool
        is_connected()
        {
            return ( ( this.connection != null ) && ( ! this.connection.is_closed() ) && this.handshake_passed );
        }

        private void
        proccess_address() throws EventcError
        {
            if ( this.address != null )
                return;

            #if HAVE_GIO_UNIX
            string path = null;
            if ( this._host == "localhost" )
                path = GLib.Path.build_filename(GLib.Environment.get_user_runtime_dir(), Eventd.Config.PACKAGE_NAME, Eventd.Config.UNIX_SOCKET);
            else if ( this.host_is_socket )
                path = this._host;
            if ( ( path != null ) && GLib.FileUtils.test(path, GLib.FileTest.EXISTS) && ( ! GLib.FileUtils.test(path, GLib.FileTest.IS_DIR|GLib.FileTest.IS_REGULAR) ) )
                this.address = new GLib.UnixSocketAddress(path);
            #endif
            if ( this.address == null )
            {
                this.address = new GLib.NetworkAddress(this._host, ( this._port > 0 ) ? ( this._port ) : ( Eventd.Config.DEFAULT_BIND_PORT ));
                if ( address == null )
                    throw new EventcError.HOSTNAME("Couldn’t resolve the hostname");
            }
        }

        private void
        set_client() throws EventcError
        {
            this.proccess_address();

            this.client.set_timeout(this.timeout);
            this.client.set_enable_proxy(this.enable_proxy);
        }

        public new async void
        connect() throws EventcError
        {
            if ( this.connection != null )
            {
                if ( this.handshake_passed )
                    throw new EventcError.ALREADY_CONNECTED("Already connected, you must disconnect first");
                else
                    yield this.close();
            }

            this.handshake_passed = false;

            while ( ! this.mutex.trylock() )
            {
                Idle.add(this.connect.callback);
                yield;
            }

            try
            {

            this.set_client();
            try
            {
                this.connection = yield this.client.connect_async(this.address);
            }
            catch ( GLib.Error e )
            {
                if ( e is GLib.IOError.CONNECTION_REFUSED )
                    throw new EventcError.CONNECTION_REFUSED("Host is not an eventd");
                else
                    throw new EventcError.CONNECTION_OTHER("Failed to connect: %s", e.message);
            }
            yield this.hello();

            }
            finally
            {
            this.mutex.unlock();
            }
        }

        private async void
        hello() throws EventcError
        {
            this.input = new GLib.DataInputStream((this.connection as GLib.IOStream).get_input_stream());
            this.output = new GLib.DataOutputStream((this.connection as GLib.IOStream).get_output_stream());

            this.send("HELLO " + this.category);

            var r = yield this.receive();
            if ( r != "HELLO" )
                throw new EventcError.HELLO("Got a wrong hello message: %s", r);
            else
                this.handshake_passed = true;
        }

        public async void
        event(Eventd.Event event) throws EventcError
        {
            while ( ! this.mutex.trylock() )
            {
                Idle.add(this.event.callback);
                yield;
            }

            try
            {

            unowned string name = event.get_name();
            this.send( @"EVENT $name");

            var event_category =  event.get_category();
            if ( (  event_category != null ) && ( this.category != event_category ) )
                this.send("CATEGORY " + event.get_category());

            unowned GLib.HashTable<string, string> data = event.get_all_data();
            if ( data != null )
            {
                EventcError e = null;
                data.foreach((name, content) => {
                    try
                    {
                        if ( content.index_of_char('\n') == -1 )
                            this.send(@"DATAL $name $content");
                        else
                        {
                            this.send(@"DATA $name");
                            var datas = content.split("\n");
                            foreach ( var line in datas )
                            {
                                if ( line[0] == '.' )
                                    line = "." + line;
                                this.send(line);
                            }
                            this.send(".");
                        }
                    }
                    catch ( EventcError ie )
                    {
                        e = ie;
                    }
                });
                if ( e != null )
                    throw e;
            }
            this.send(".");
            var r = yield this.receive();
            if ( r != "OK" )
                throw new EventcError.EVENT("Got a wrong event acknowledge message: %s", r);

            }
            finally
            {
            this.mutex.unlock();
            }
        }

        private async string?
        receive() throws EventcError
        {
            string r = null;
            try
            {
                r = yield this.input.read_upto_async("\n", -1);
                this.input.read_byte(null);
            }
            catch ( GLib.Error e )
            {
                this.handshake_passed = false;
                throw new EventcError.RECEIVE("Failed to receive message: %s", e.message);
            }
            return r;
        }

        private void
        send(string msg) throws EventcError
        {
            try
            {
                this.output.put_string(msg + "\n", null);
            }
            catch ( GLib.Error e )
            {
                this.handshake_passed = false;
                throw new EventcError.SEND("Couldn’t send message \"%s\": %s", msg, e.message);
            }
        }

        public async void
        close() throws EventcError
        {
            while ( ! this.mutex.trylock() )
            {
                Idle.add(this.close.callback);
                yield;
            }

            if ( ( this.handshake_passed ) && ( ! this.connection.is_closed() ) )
            {
                try
                {
                    this.send("BYE");
                }
                catch ( EventcError ee ) {}
            }
            try
            {
                yield this.connection.close_async(GLib.Priority.DEFAULT);
            }
            catch ( GLib.Error e ) {}

            this.output = null;
            this.input = null;
            this.connection = null;

            this.mutex.unlock();
        }
    }
}
