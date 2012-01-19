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

    public class Connection : GLib.Object
    {
        public enum Mode {
            UNKNOWN = -1,
            NORMAL = 0,
            PING_PONG
        }

        private GLib.Mutex mutex;

        private string _host;
        private uint16 _port;
        private string _category;
        public Mode mode;

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

        public string category
        {
            set
            {
                if ( this._category != value )
                    this._category = value;
            }
        }

        #if ENABLE_GIO_UNIX
        public bool host_is_socket { get; set; default = false; }
        #endif

        public uint timeout { get; set; default = 0; }
        public bool enable_proxy { get; set; default = true; }

        private GLib.SocketConnectable address;
        private GLib.SocketClient client;
        private GLib.SocketConnection connection;
        private GLib.DataInputStream input;
        private GLib.DataOutputStream output;

        private bool hello_received;

        public
        Connection(string host, uint16 port, string category)
        {
            this.mutex = new GLib.Mutex();
            this._host = host;
            this._port = port;
            this._category = category;
            this.mode = Mode.UNKNOWN;

            this.client = new GLib.SocketClient();
        }

        public bool
        is_connected()
        {
            return ( ( this.connection != null ) && ( ! this.connection.is_closed() ) && this.hello_received );
        }

        private void
        proccess_address() throws EventcError
        {
            if ( this.address != null )
                return;

            #if ENABLE_GIO_UNIX
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
                if ( this.hello_received )
                    throw new EventcError.ALREADY_CONNECTED("Already connected, you must disconnect first");
                else
                    yield this.close();
            }

            this.hello_received = false;

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

            this.send("HELLO " + this._category);

            var r = yield this.receive();
            if ( r != "HELLO" )
                throw new EventcError.HELLO("Got a wrong hello message: %s", r);
            else
                this.hello_received = true;

            yield this.send_mode();
        }

        private async void
        send_mode() throws EventcError
        {
            unowned string mode = null;
            switch ( this.mode )
            {
            case Mode.NORMAL:
                mode = "normal";
            break;
            case Mode.PING_PONG:
                mode = "ping-pong";
            break;
            default:
                throw new EventcError.MODE("Please specify a mode");
            }

            this.send("MODE " + mode);

            var r = yield this.receive();
            if ( r != "MODE" )
                throw new EventcError.MODE("Got a wrong mode message: %s", r);
        }

        public async void
        rename() throws EventcError
        {
            this.send("RENAME " + this._category);
            var r = yield this.receive();
            if ( r != "RENAMED" )
                throw new EventcError.RENAMED("Got a wrong renamed message: %s", r);
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

            unowned GLib.HashTable<string, string> data = event.get_data();
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

            string line = null;
            string data_name = null;
            string data_content = null;
            switch ( this.mode )
            {
            case Mode.PING_PONG:
                while ( ( line = yield this.receive() ) != null )
                {
                    if ( line == "EVENT" )
                    {}
                    else if ( line == "." )
                    {
                        if ( data_name != null )
                            event.add_pong_data((owned)data_name, (owned)data_content);
                        else
                            break;
                    }
                    else if ( line.ascii_ncasecmp("DATAL ", 6) == 0 )
                    {
                        var data_line = line.substring(6).split(" ", 2);
                        event.add_pong_data(data_line[0], data_line[1]);
                    }
                    else if ( line.ascii_ncasecmp("DATA ", 5) == 0 )
                    {
                        data_name = line.substring(5);
                        data_content = "";
                    }
                    else if ( data_name != null )
                    {
                        data_content += line;
                    }
                }
            break;
            default:
            break;
            }

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

            try
            {

            if ( ( this.hello_received ) && ( ! this.connection.is_closed() ) )
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

            }
            finally
            {
            this.mutex.unlock();
            }
        }
    }
}
