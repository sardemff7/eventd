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
    public class Eventc : GLib.Object
    {
        public enum Error
        {
            NONE = 0,
            HOSTNAME,
            CONNECTION,
            HELLO,
            RENAMED
        }

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
                    this.connect();
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
                    this.connect();
                }
            }
        }

        public string client_type
        {
            set
            {
                if ( this._type != value )
                {
                    this._type = value;
                    this.rename();
                }
            }
        }

        public string client_name
        {
            set
            {
                if ( this._name != value )
                {
                    this._name = value;
                    this.rename();
                }
            }
        }

        private uint64 tries;
        public uint64 max_tries { get; set; default = 3; }


        private GLib.SocketClient client;
        private GLib.SocketConnection connection;
        private GLib.DataInputStream input;
        private GLib.DataOutputStream output;

        public Eventc(string host, uint16 port, string type, string? name)
        {
            this._host = host;
            this._port = port;
            this._type = type;
            this._name = name;
            this.tries = 0;
        }

        public new Error
        connect()
        {
            if ( this.client != null )
                this.close();
            GLib.SocketAddress address = null;
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
                address = new GLib.UnixSocketAddress(path);
            #endif
            if ( address == null )
            {
                var inet_address = new GLib.InetAddress.from_string(this._host);
                if ( inet_address == null )
                {
                    GLib.warning("Couldn’t parse the hostname");
                    return Error.HOSTNAME;
                }
                address = new GLib.InetSocketAddress(inet_address, ( this._port > 0 ) ? ( this._port ) : ( Common.DEFAULT_BIND_PORT ));
            }
            this.client = new GLib.SocketClient();

            try
            {
                this.connection = this.client.connect(address);
            }
            catch ( GLib.Error e )
            {
                GLib.warning("Failed to connect: %s", e.message);
                if ( ++this.tries >= this.max_tries )
                    GLib.error("Failed %llu times, aborting", this.tries);
                this.connect();
                return Error.CONNECTION;
            }

            this.input = new GLib.DataInputStream((this.connection as GLib.IOStream).get_input_stream());
            this.output = new GLib.DataOutputStream((this.connection as GLib.IOStream).get_output_stream());

            this.tries = 0;
            if ( this._name == null )
                this.send("HELLO %s".printf(this._type));
            else
                this.send("HELLO %s %s".printf(this._type, this._name));
            var r = this.receive(null);
            if ( r != "HELLO" )
            {
                GLib.warning("Got a wrong hello message: %s", r);
                return Error.HELLO;
            }
            return Error.NONE;
        }

        private Error
        rename()
        {
            if ( this._name == null )
                this.send("RENAME %s".printf(this._type));
            else
                this.send("RENAME %s %s".printf(this._type, this._name));
            var r = this.receive(null);
            if ( r != "RENAMED" )
            {
                GLib.warning("Got a wrong renamed message: %s", r);
                return Error.RENAMED;
            }
            return Error.NONE;
        }

        public void event(string type, string? name, string? data)
        {
            if ( name == null )
                send("EVENT %s".printf(type));
            else
                send("EVENT %s %s".printf(type, name));
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

        private string? receive(out size_t? length)
        {
            string r = null;
            try
            {
                r = this.input.read_upto("\n", -1, out length);
                this.input.read_byte(null);
            }
            catch ( GLib.Error e )
            {
                GLib.warning("Failed to receive message: %s", e.message);
                this.connect();
                length = 0;
            }
            return r;
        }

        private void send(string msg)
        {
            try
            {
                this.output.put_string("%s\n".printf(msg), null);
            }
            catch ( GLib.Error e )
            {
                GLib.warning("Failed to send message \"%s\": %s", msg, e.message);
                this.connect();
                this.send(msg);
            }
        }

        public void close()
        {
            if ( ! this.connection.is_closed() )
            {
                this.send("BYE");
                var r = this.receive(null);
                if ( r != "BYE" )
                    GLib.warning("Got a wrong bye message: %s", r);
            }
            try
            {
                this.connection.close(null);
            }
            catch ( GLib.Error e )
            {
                GLib.warning("Failed to close socket: %s", e.message);
            }
            this.output = null;
            this.input = null;
            this.connection = null;
        }
    }
}
