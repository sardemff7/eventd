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
    [CCode (cheader_filename = "libeventd-event-private.h", cname = "EventdEvent", cprefix = "eventd_event_", type_id = "EVENTD_TYPE_EVENT")]
    public class PrivateEvent : Event
    {
        public void set_all_data(GLib.HashTable<string, string> data);
        public void set_all_answer_data(GLib.HashTable<string, string> data);
        public unowned GLib.List<string> get_answers();
        public unowned GLib.HashTable<string, string> get_all_data();
        public unowned GLib.HashTable<string, string> get_all_answer_data();
    }
}
