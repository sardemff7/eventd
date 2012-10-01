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

namespace Eventd.Tests
{
    public class Env
    {
        private string[] env;
        private string[] start_args;
        private string[] stop_args;

        public static void
        setup()
        {
            GLib.Environment.set_variable("EVENTD_CONFIG_DIR", GLib.Path.build_filename(BUILD_DIR, "tests"), true);
            GLib.Environment.set_variable("XDG_RUNTIME_DIR", GLib.Path.build_filename(BUILD_DIR, "tests"), true);
            GLib.Environment.set_variable("EVENTD_PLUGINS_DIR", GLib.Path.build_filename(BUILD_DIR, Config.LT_OBJDIR), true);
        }

        public
        Env(string plugins, string port, ...)
        {
            this.env = GLib.Environ.get();
            this.env = GLib.Environ.set_variable((owned) this.env, "EVENTD_PLUGINS_WHITELIST", plugins, true);

            var eventd_path = GLib.Path.build_filename(BUILD_DIR, "eventd" + EXEEXT);
            var eventdctl_path = GLib.Path.build_filename(BUILD_DIR, "eventdctl" + EXEEXT);

            this.start_args = { eventdctl_path, "--socket", port, "start", "--argv0", eventd_path, "--take-over", "--private-socket", port };
            var l = va_list();
            string arg = null;
            while ( ( arg = l.arg() ) != null )
                this.start_args += arg;

            this.stop_args = { eventdctl_path, "--socket", port, "quit" };
        }

        public void
        start_eventd() throws GLib.Error
        {
            GLib.Process.spawn_sync(BUILD_DIR, this.start_args, this.env, 0, null);
        }

        public void
        stop_eventd() throws GLib.Error
        {
            GLib.Process.spawn_sync(BUILD_DIR, this.stop_args, this.env, 0, null);
        }
    }
}
