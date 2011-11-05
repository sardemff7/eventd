/*
 * libeventc - Library to communicate with eventd
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
    public errordomain EventcError
    {
        HOSTNAME,
        CONNECTION_REFUSED,
        CONNECTION_OTHER,
        HELLO,
        MODE,
        BYE,
        RENAMED,
        SEND,
        RECEIVE,
        EVENT,
        CLOSE
    }

    public class Eventc : GLib.Object
    {
        public enum Mode {
            UNKNOWN = -1,
            NORMAL = 0,
            PING_PONG
        }

        private string _host;
        private uint16 _port;
        private string _type;
        private string _name;
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

        public string client_type
        {
            set
            {
                if ( this._type != value )
                    this._type = value;
            }
        }

        public string client_name
        {
            set
            {
                if ( this._name != value )
                    this._name = value;
            }
        }

        #if ENABLE_GIO_UNIX
        public bool host_is_socket { get; set; default = false; }
        #endif

        public uint timeout { get; set; default = 0; }

        private GLib.SocketConnectable address;
        private GLib.SocketClient client;
        private GLib.SocketConnection connection;
        private GLib.DataInputStream input;
        private GLib.DataOutputStream output;

        private bool hello_received;

        public
        Eventc(string host, uint16 port, string type, string? name)
        {
            this._host = host;
            this._port = port;
            this._type = type;
            this._name = name;
            this.mode = Mode.UNKNOWN;
        }

        private void
        proccess_address() throws EventcError
        {
            if ( this.address != null )
                return;

            this.hello_received = false;

            #if ENABLE_GIO_UNIX
            string path = null;
            if ( this._host == "localhost" )
                path = GLib.Path.build_filename(GLib.Environment.get_user_runtime_dir(), Config.PACKAGE_NAME, Common.UNIX_SOCKET);
            else if ( this.host_is_socket )
                path = this._host;
            if ( ( path != null ) && GLib.FileUtils.test(path, GLib.FileTest.EXISTS) && ( ! GLib.FileUtils.test(path, GLib.FileTest.IS_DIR|GLib.FileTest.IS_REGULAR) ) )
                this.address = new GLib.UnixSocketAddress(path);
            #endif
            if ( this.address == null )
            {
                this.address = new GLib.NetworkAddress(this._host, ( this._port > 0 ) ? ( this._port ) : ( Common.DEFAULT_BIND_PORT ));
                if ( address == null )
                    throw new EventcError.HOSTNAME("Couldn’t resolve the hostname");
            }
        }

        public bool
        is_connected()
        {
            return ( ( this.connection != null ) && ( ! this.connection.is_closed() ) && this.hello_received );
        }

        private void
        set_client() throws EventcError
        {
            if ( this.connection != null )
                this.close();

            this.proccess_address();

            this.client = new GLib.SocketClient();
            this.client.set_timeout(this.timeout);
        }

        public new void
        connect() throws EventcError
        {
            this.set_client();
            try
            {
                this.connection = this.client.connect(this.address);
            }
            catch ( GLib.Error e )
            {
                if ( e is GLib.IOError.CONNECTION_REFUSED )
                    throw new EventcError.CONNECTION_REFUSED("Host is not an eventd");
                else
                    throw new EventcError.CONNECTION_OTHER("Failed to connect: %s", e.message);
            }
            this.hello();
        }

        public async void
        connect_async() throws EventcError
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
            yield this.hello_async();
        }

        private void
        hello_common() throws EventcError
        {
            this.input = new GLib.DataInputStream((this.connection as GLib.IOStream).get_input_stream());
            this.output = new GLib.DataOutputStream((this.connection as GLib.IOStream).get_output_stream());

            if ( this._name == null )
                this.send("HELLO " + this._type);
            else
                this.send("HELLO " + this._type + " " + this._name);
        }

        private async void
        hello_async() throws EventcError
        {
            this.hello_common();

            var r = yield this.receive_async();
            if ( r != "HELLO" )
                throw new EventcError.HELLO("Got a wrong hello message: %s", r);
            else
                this.hello_received = true;

            yield this.send_mode_async();
        }

        private void
        hello() throws EventcError
        {
            this.hello_common();

            var r = this.receive();
            if ( r != "HELLO" )
                throw new EventcError.HELLO("Got a wrong hello message: %s", r);
            else
                this.hello_received = true;

            this.send_mode();
        }

        private void
        send_mode_common() throws EventcError
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
            }
            if ( mode != null )
            this.send("MODE " + mode);
        }

        private async void
        send_mode_async() throws EventcError
        {
            this.send_mode_common();

            var r = yield this.receive_async();
            if ( r != "MODE" )
                throw new EventcError.MODE("Got a wrong mode message: %s", r);
        }

        private void
        send_mode() throws EventcError
        {
            this.send_mode_common();

            var r = this.receive();
            if ( r != "MODE" )
                throw new EventcError.MODE("Got a wrong mode message: %s", r);
        }

        public void
        rename() throws EventcError
        {
            if ( this._name == null )
                this.send("RENAME " + this._type);
            else
                this.send("RENAME " + this._type +  " " + this._name);
            var r = this.receive();
            if ( r != "RENAMED" )
                throw new EventcError.RENAMED("Got a wrong renamed message: %s", r);
        }

        public void
        event_common(string type, GLib.HashTable<string, string>? data) throws EventcError
        {
            this.send( @"EVENT $type");

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
        }

        public GLib.HashTable<string, string>?
        event(string type, GLib.HashTable<string, string>? data) throws EventcError
        {
            this.event_common(type, data);
            GLib.HashTable<string, string>? ret = null;
            string line = null;
            string data_name = null;
            string data_content = null;
            switch ( this.mode )
            {
            case Mode.PING_PONG:
                while ( ( line = this.receive() ) != null )
                {
                    if ( line == "EVENT" )
                        ret = new GLib.HashTable<string, string>(string.hash, GLib.str_equal);
                    else if ( line == "." )
                    {
                        if ( data_name != null )
                            ret.insert((owned)data_name, (owned)data_content);
                        else
                            break;
                    }
                    else if ( ret == null )
                        throw new EventcError.EVENT("Can’t receive correct event data");
                    else if ( line.ascii_ncasecmp("DATAL ", 6) == 0 )
                    {
                        var data_line = line.substring(6).split(" ", 2);
                        ret.insert(data_line[0], data_line[1]);
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
            return (owned)ret;
        }

        private string?
        receive() throws EventcError
        {
            string r = null;
            try
            {
                r = this.input.read_upto("\n", -1, null);
                this.input.read_byte(null);
            }
            catch ( GLib.Error e )
            {
                throw new EventcError.RECEIVE("Failed to receive message: %s", e.message);
            }
            return r;
        }

        private async string?
        receive_async() throws EventcError
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

        public void
        close() throws EventcError
        {
            if ( ( this.hello_received ) && ( ! this.connection.is_closed() ) )
            {
                this.send("BYE");
                var r = this.receive();
                if ( r != "BYE" )
                    throw new EventcError.BYE("Got a wrong bye message: %s", r);
            }
            try
            {
                this.connection.close(null);
            }
            catch ( GLib.Error e )
            {
                throw new EventcError.CLOSE("Failed to close socket: %s", e.message);
            }
            this.output = null;
            this.input = null;
            this.connection = null;
            this.client = null;
        }
    }
}
