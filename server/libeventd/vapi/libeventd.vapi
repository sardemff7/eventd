/*
 * libeventd-event - Library to manipulate eventd events
 *
 * Copyright © 2011-2015 Quentin "Sardem FF7" Glidic
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with eventd. If not, see <http://www.gnu.org/licenses/>.
 *
 */

namespace Eventd
{
    [CCode (cheader_filename = "libeventd-event.h")]
    public enum EventEndReason
    {
        NONE,
        DISCARD,
        TIMEOUT,
        USER_DISMISS,
        CLIENT_DISMISS,
        TEST,
        UNKNOWN,
        RESERVED;
    }

    [CCode (cheader_filename = "libeventd-event.h")]
    public class Event : GLib.Object
    {
        [CCode (has_construct_function = false)]
        public Event(string category, string name);

        public void set_timeout(int64 timeout);
        public void add_data(owned string name, owned string content);
        public void add_answer(string name);
        public void add_answer_data(owned string name, owned string content);

        public unowned string get_category();
        public unowned string get_name();
        public int64 get_timeout();
        public bool has_data(string name);
        public unowned string get_data(string name);
        public GLib.HashTable<weak string,weak string>? get_all_data();
        public unowned string get_answer_data(string name);
        public GLib.HashTable<weak string,weak string>? get_all_answer_data();

        public void update(string name);
        public void answer(string answer);
        public void end(EventEndReason reason);

        public signal void updated();
        public signal void answered(string answer);
        public signal void ended(EventEndReason reason);
    }

    [CCode (cheader_filename = "libeventd-protocol.h")]
    public errordomain ProtocolParseError
    {
        UNEXPECTED_TOKEN,
        MALFORMED,
        GARBAGE,
        WRONG_UUID,
        KNOWN_ID,
        UNKNOWN_ID,
        UNKNOWN;
    }

    [CCode (cheader_filename = "libeventd-protocol.h")]
    public interface Protocol : GLib.Object
    {
        public abstract bool parse(string buffer) throws ProtocolParseError;

        public abstract string generate_answered(Eventd.Event event, string answer);
        public abstract string generate_bye(string? message);
        public abstract string generate_ended(Eventd.Event event, Eventd.EventEndReason reason);
        public abstract string generate_event(Eventd.Event event);
        public abstract string generate_passive();

        public signal void event(Eventd.Event event);
        public signal void answered(Eventd.Event event, string answer);
        public signal void ended(Eventd.Event event, Eventd.EventEndReason reason);
        public signal void passive();
        public signal void bye(string? message);
    }

    [CCode (cheader_filename = "libeventd-protocol.h")]
    public class ProtocolEvp : GLib.Object, Eventd.Protocol
    {
        [CCode (has_construct_function = false, type = "EventdProtocol*")]
        public ProtocolEvp();
    }
}
