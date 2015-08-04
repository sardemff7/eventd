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

namespace Eventd
{
    [CCode (cheader_filename = "libeventd-event.h")]
    public enum EventEndReason
    {
        NONE = 0,
        TIMEOUT = 1,
        USER_DISMISS = 2,
        CLIENT_DISMISS = 3,
        RESERVED = 4
    }

    [CCode (cheader_filename = "libeventd-event.h")]
    public class Event : GLib.Object
    {
        public Event(string category, string name);

        public void add_answer(string answer);
        public void add_data(owned string name, owned string content);
        public void add_answer_data(owned string name, owned string content);

        public unowned string get_category();
        public unowned string get_name();
        public unowned string get_data(string name);

        public void answer(string answer);
        public void end(EventEndReason reason);

        public signal void answered(string answer);
        public signal void ended(EventEndReason reason);

        /*
         * Special accessors meant for plugins
         * Do *not* use in regular client code
         */
        public void set_all_data(owned GLib.HashTable<string, string> data);
        public void set_all_answer_data(owned GLib.HashTable<string, string> data);
        public unowned GLib.List<string> get_answers();
        public unowned GLib.HashTable<string, string> get_all_data();
        public unowned GLib.HashTable<string, string> get_all_answer_data();
    }
}
