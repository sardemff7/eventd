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
	[CCode (cheader_filename = "libeventc.h")]
	public static unowned string get_version();

	[CCode (cheader_filename = "libeventc.h")]
	public errordomain Error {
		HOSTNAME,
		CONNECTION,
		ALREADY_CONNECTED,
		NOT_CONNECTED,
		RECEIVE,
		EVENT,
		END,
		BYE
	}

	[CCode (cheader_filename = "libeventc.h")]
	public class Connection : GLib.Object
	{
		public Connection(string host) throws Eventc.Error;
		public Connection.for_connectable(GLib.SocketConnectable connectable);

		public bool get_passive();
		public bool get_enable_proxy();
		public bool get_subscribe();

		public bool set_host(string host) throws Eventc.Error;
		public void set_connectable(GLib.SocketConnectable address);
		public void set_accept_unknown_ca(bool accept_unknown_ca);
		public void set_passive(bool passive);
		public void set_enable_proxy(bool enable_proxy);
		public void set_subscribe(bool subscribe);
		public void add_subscription(owned string category);

		public bool is_connected() throws Eventc.Error;
		public new async void connect() throws Eventc.Error;
		public new void connect_sync() throws Eventc.Error;
		public bool event(Eventd.Event event) throws Eventc.Error;
		public bool close() throws Eventc.Error;
	}
}
