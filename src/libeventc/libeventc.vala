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
        private string host;
        private uint16 port;
        private string type;
        private string name;
        private uint64 tries;
        public uint64 max_tries { get; set; default = 3; }


        private GLib.SocketClient client;
        private GLib.SocketConnection connection;
        private GLib.DataInputStream input;
        private GLib.DataOutputStream output;

        public Eventc(string host, uint16 port, string type, string? name)
        {
            this.host = host;
            this.port = port;
            this.type = type;
            this.name = name;
            this.tries = 0;
        }

        public new void connect()
        {
            GLib.SocketAddress address = null;
            #if ENABLE_GIO_UNIX
            string path = null;
            if ( this.host[0] == '/' )
                path = this.host;
            string runtime_dir = "%s/%s".printf(GLib.Environment.get_user_runtime_dir(), Config.PACKAGE);
            if ( ( this.host[0] == '%' ) && ( this.host[1] == 's' ) && ( this.host[2] == '/' ) )
                path = this.host.printf(runtime_dir);
            else if ( this.host == "localhost" )
                path = Common.UNIX_SOCKET.printf(runtime_dir);
            if ( ( path != null ) && ( GLib.FileUtils.test(path, GLib.FileTest.EXISTS|GLib.FileTest.IS_REGULAR) ) )
                address = new GLib.UnixSocketAddress(path);
            #endif
            if ( address == null )
            {
                var inet_address = new GLib.InetAddress.from_string(this.host);
                address = new GLib.InetSocketAddress(inet_address, ( this.port > 0 ) ? ( this.port ) : ( Common.DEFAULT_BIND_PORT ));
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
                return;
            }

            this.input = new GLib.DataInputStream((this.connection as GLib.IOStream).get_input_stream());
            this.output = new GLib.DataOutputStream((this.connection as GLib.IOStream).get_output_stream());

            this.tries = 0;
            if ( this.name == null )
                this.send("HELLO %s".printf(this.type));
            else
                this.send("HELLO %s %s".printf(this.type, this.name));
            var r = this.receive(null);
            if ( r != "HELLO" )
                GLib.warning("Got a wrong hello message: %s", r);
        }

        public void event(string type, string? name, string? data)
        {
            if ( name == null )
                send("EVENT %s".printf(type));
            else
                send("EVENT %s %s".printf(type, name));
            if ( data != null )
                this.send(data);
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
            this.connection = null;
        }
    }
}
