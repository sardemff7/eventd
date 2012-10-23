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

[CCode (cheader_filename = "libeventd-evp.h")]
namespace Libeventd
{
    namespace Evp
    {
        GLib.SocketConnectable? get_address(string hostname, uint16 port);

        [SimpleType]
        [CCode (ref_function = "", unref_function = "")]
        public class Context
        {
            public Context(void *client, ref ClientInterface client_interface);
            public Context.for_connection(void *client, ref ClientInterface client_interface, GLib.SocketConnection connection);

            public bool is_connected() throws GLib.Error;

            public void set_connection(GLib.SocketConnection connection);
            public async void close();

            public bool @lock(GLib.SourceFunc @callback);
            public void unlock();

            public void receive_loop_client(int priority = GLib.Priority.DEFAULT);
            public void receive_loop_server(int priority = GLib.Priority.DEFAULT);

            public async bool send_hello(string category) throws GLib.Error;

            public async unowned string? send_event(Eventd.Event event) throws GLib.Error;

            public async bool send_end(string id) throws GLib.Error;

            public void send_bye();
        }

        [CCode (has_target = false)]
        public delegate void *EventGetter(void *client, Context context, string id);
        [CCode (has_target = false)]
        public delegate unowned void Callback(void *client, Context context);
        [CCode (has_target = false)]
        public delegate unowned void ErrorCallback(void *client, Context context, GLib.Error error);
        [CCode (has_target = false)]
        public delegate unowned string HelloCallback(void *client, Context context, string category);
        [CCode (has_target = false)]
        public delegate string EventCallback(void *client, Context context, Eventd.Event event);
        [CCode (has_target = false)]
        public delegate void EndCallback(void *client, Context context, void *event);
        [CCode (has_target = false)]
        public delegate void AnsweredCallback(void *client, Context context, void *event, string answer, GLib.HashTable<string, string> data_hashS);
        [CCode (has_target = false)]
        public delegate void EndedCallback(void *client, Context context, void *event, Eventd.EventEndReason reason);

        public struct ClientInterface
        {
            EventGetter get_event;
            ErrorCallback error;

            HelloCallback hello;
            EventCallback event;
            EndCallback end;

            AnsweredCallback answered;
            EndedCallback ended;

            Callback bye;
        }
    }
}
