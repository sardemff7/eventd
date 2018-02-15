/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2018 Quentin "Sardem FF7" Glidic
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

#include "config.h"

#include <glib.h>
#include <glib/gprintf.h>

#include "libeventc.h"
#include "libeventd-test.h"

int
main(int argc, char *argv[])
{
    int r = 99;
    eventd_tests_env_setup(argv, "ws-connection");
    EventdTestsEnv *env = eventd_tests_env_new(NULL, NULL, FALSE);
    if ( ! eventd_tests_env_start_eventd(env) )
        goto end;

    gchar *file;
    gchar *port;
    gsize length;
    file = g_build_filename(g_get_user_runtime_dir(), PACKAGE_NAME, EVP_UNIX_SOCKET, NULL);
    if ( g_file_test(file, G_FILE_TEST_IS_REGULAR) && g_file_get_contents(file, &port, &length, NULL) && ( length < 6 ) )
    {
        gchar uri[21];
        g_snprintf(uri, sizeof(uri), "ws://localhost:%s", port);
        r = eventd_tests_run_libeventc(uri);
    }
    g_free(file);


    if ( ! eventd_tests_env_stop_eventd(env) )
        r = 99;

end:
    eventd_tests_env_free(env);
    return r;
}
