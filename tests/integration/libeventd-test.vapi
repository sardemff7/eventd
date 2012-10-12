/*
 * eventd - Small daemon to act on remote or local events
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

[CCode (cheader_filename = "libeventd-test.h")]
namespace Eventd.Tests
{
    [CCode (ref_function = "", unref_function = "")]
    public class Env
    {
        public Env(string plugins, string port, owned string[] args);
        public static void setup();
        public void start_eventd() throws GLib.Error;
        public void stop_eventd() throws GLib.Error;
    }
}
