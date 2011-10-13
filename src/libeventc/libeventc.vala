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
        BYE,
        RENAMED,
        SEND,
        RECEIVE,
        CLOSE
    }

    public class Eventc : GLib.Object
    {

        private string _host;
        private uint16 _port;
        private string _type;
        private string _name;

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

        public uint timeout { get; set; default = 0; }

        private GLib.SocketConnectable address;
        private GLib.SocketClient client;
        private GLib.SocketConnection connection;
        private GLib.DataInputStream input;
        private GLib.DataOutputStream output;

        public
        Eventc(string host, uint16 port, string type, string? name)
        {
            this._host = host;
            this._port = port;
            this._type = type;
            this._name = name;
        }

        private void
        proccess_address() throws EventcError
        {
            if ( this.address != null )
                return;

            #if ENABLE_GIO_UNIX
            string path = null;
            if ( this._host[0] == '/' )
                path = this._host;
            string runtime_dir = "%s/%s".printf(GLib.Environment.get_user_runtime_dir(), Config.PACKAGE);
            if ( ( this._host[0] == '%' ) && ( this._host[1] == 's' ) && ( this._host[2] == '/' ) )
                path = this._host.printf(runtime_dir);
            else if ( this._host == "localhost" )
                path = Common.UNIX_SOCKET.printf(runtime_dir);
            if ( ( path != null ) && ( GLib.FileUtils.test(path, GLib.FileTest.EXISTS|GLib.FileTest.IS_REGULAR) ) )
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
            return ( ( this.connection != null ) && ( ! this.connection.is_closed() ) );
        }

        public new void
        connect() throws EventcError
        {
            if ( this.connection != null )
                this.close();

            this.proccess_address();

            this.client = new GLib.SocketClient();
            this.client.set_timeout(this.timeout);

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

            this.input = new GLib.DataInputStream((this.connection as GLib.IOStream).get_input_stream());
            this.output = new GLib.DataOutputStream((this.connection as GLib.IOStream).get_output_stream());

            if ( this._name == null )
                this.send("HELLO %s".printf(this._type));
            else
                this.send("HELLO %s %s".printf(this._type, this._name));
            var r = this.receive(null);
            if ( r != "HELLO" )
            {
                throw new EventcError.HELLO("Got a wrong hello message: %s", r);
            }
        }

        public void
        rename() throws EventcError
        {
            if ( this._name == null )
                this.send("RENAME %s".printf(this._type));
            else
                this.send("RENAME %s %s".printf(this._type, this._name));
            var r = this.receive(null);
            if ( r != "RENAMED" )
                throw new EventcError.RENAMED("Got a wrong renamed message: %s", r);
        }

        public void
        event(string type, string? name, string? data) throws EventcError
        {
            string ev = null;
            if ( name == null )
                ev = @"EVENT $type";
            else
                ev = @"EVENT $type $name";
            this.send(ev);

            if ( data != null )
            {
                var datas = data.split("\n");
                foreach ( var line in datas )
                {
                    if ( line[0] == '.' )
                        line = "." + line;
                    this.send(line);
                }
            }
            this.send(".");
        }

        private string?
        receive(out size_t? length) throws EventcError
        {
            string r = null;
            try
            {
                r = this.input.read_upto("\n", -1, out length);
                this.input.read_byte(null);
            }
            catch ( GLib.Error e )
            {
                length = 0;
                throw new EventcError.RECEIVE("Failed to receive message: %s", e.message);
            }
            return r;
        }

        private void
        send(string msg) throws EventcError
        {
            try
            {
                this.output.put_string("%s\n".printf(msg), null);
            }
            catch ( GLib.Error e )
            {
                throw new EventcError.SEND("Couldn’t send message \"%s\": %s", msg, e.message);
            }
        }

        public void
        close() throws EventcError
        {
            if ( ! this.connection.is_closed() )
            {
                this.send("BYE");
                var r = this.receive(null);
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
