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
    public static void
    start_eventd(string plugins, ...) throws GLib.Error
    {
        string[] args = { GLib.Path.build_filename(Eventd.Tests.BUILD_DIR, "eventdctl" + Eventd.Tests.EXEEXT), "--socket=9987" , "start", "--argv0" , GLib.Path.build_filename(Eventd.Tests.BUILD_DIR, "eventd" + Eventd.Tests.EXEEXT), "--take-over", "--private-socket=9987" };
        var l = va_list();
        string arg = null;
        while ( ( arg = l.arg() ) != null )
            args += arg;
        GLib.Environment.set_variable("XDG_RUNTIME_DIR", GLib.Path.build_filename(Eventd.Tests.BUILD_DIR, "tests"), true);
        GLib.Environment.set_variable("EVENTD_PLUGINS_DIR", GLib.Path.build_filename(Eventd.Tests.BUILD_DIR, Eventd.Config.LT_OBJDIR), true);
        GLib.Environment.set_variable("EVENTD_PLUGINS_WHITELIST", plugins, true);
        GLib.Process.spawn_sync(null,
                                 args,
                                 null,
                                 0,
                                 null);
    }

    public static void
    stop_eventd() throws GLib.Error
    {
        GLib.Process.spawn_sync(null,
                                { GLib.Path.build_filename(Eventd.Tests.BUILD_DIR, "eventdctl" + Eventd.Tests.EXEEXT), "--socket=9987" , "quit"},
                                null,
                                0,
                                null);
    }
}
