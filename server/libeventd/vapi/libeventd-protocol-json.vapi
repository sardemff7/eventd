/*
 * libeventd-protocol-json - eventd JSON protocol library
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
    [CCode (cheader_filename = "libeventd-protocol-json.h")]
    public class ProtocolJson : GLib.Object, Eventd.Protocol
    {
        [CCode (has_construct_function = false, type = "EventdProtocol*")]
        public ProtocolJson();
    }
}
