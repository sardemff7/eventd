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

#include <errno.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "libeventd-test.h"

struct _EventdTestsEnv {
    const gchar *dir;
    gchar **env;
    gchar **start_args;
    gchar **stop_args;
};

void
eventd_tests_env_setup(gchar **argv, const gchar *test)
{
#ifdef EVENTD_DEBUG_OUTPUT
    g_setenv("G_MESSAGES_DEBUG", "all", FALSE);
#endif /* EVENTD_DEBUG_OUTPUT */

    gchar *tmp, *pwd;
    tmp = g_path_get_basename(argv[0]);
    g_set_prgname(tmp);
    g_free(tmp);

    pwd = g_get_current_dir();

    const gchar *tmp_dir;
    tmp_dir = g_getenv("EVENTD_TESTS_TMP_DIR");

    if ( tmp_dir == NULL )
        tmp = g_build_filename(pwd, ".test-run", test, NULL);
    else
        tmp = g_build_filename(tmp_dir, test, NULL);
    tmp_dir = tmp;

    g_setenv("HOME", tmp_dir, TRUE);
    g_setenv("XDG_RUNTIME_DIR", tmp_dir, TRUE);

    if ( ( ! g_file_test(tmp_dir, G_FILE_TEST_IS_DIR) ) && ( g_mkdir_with_parents(tmp_dir, 0755) < 0 ) )
        g_warning("Couldn't create the test temp dir '%s': %s", tmp_dir, g_strerror(errno));

    g_free(tmp);

    g_setenv("EVENTD_CONFIG_PATH", SRC_DIR G_DIR_SEPARATOR_S "plugins" G_DIR_SEPARATOR_S "test-plugin" G_DIR_SEPARATOR_S "config", TRUE);

    g_setenv("EVENTD_PLUGINS_PATH", TEST_PLUGIN_PATH, TRUE);
    g_setenv("EVENTD_MODULES_PATH", MODULES_BUILD_DIR, TRUE);

    g_unsetenv("EVENTD_PLUGINS_WHITELIST");
    g_unsetenv("EVENTD_PLUGINS_BLACKLIST");

    g_free(pwd);
}

EventdTestsEnv *
eventd_tests_env_new(const gchar *evp_socket, const gchar *plugins, gboolean enable_relay)
{
    static guint instance = 0;
    EventdTestsEnv *self;

    self = g_new0(EventdTestsEnv, 1);

    self->dir = g_getenv("XDG_RUNTIME_DIR");

    if ( plugins == NULL )
        plugins = "test-plugin";

    self->env = g_get_environ();
    self->env = g_environ_setenv(self->env, "EVENTD_PLUGINS_WHITELIST", plugins, TRUE);

    gchar *socket = g_strdup_printf("%s-%u.private", g_get_prgname(), ++instance);

    gchar *socket_path = g_build_filename(self->dir, socket, NULL);
    g_unlink(socket_path);
    g_free(socket_path);

    if ( evp_socket == NULL )
        evp_socket = "tcp-file-runtime:" EVP_UNIX_SOCKET;

    self->start_args = g_new0(char *, 14);
    self->start_args[0] = g_strdup(EVENTDCTL_PATH);
    self->start_args[1] = g_strdup("--socket");
    self->start_args[2] = socket;
    self->start_args[3] = g_strdup("start");
    self->start_args[4] = g_strdup("--argv0");
    self->start_args[5] = g_strdup(EVENTD_PATH);
    self->start_args[6] = g_strdup("--take-over");
    self->start_args[7] = g_strdup("--private-socket");
    self->start_args[8] = g_strdup(socket);
    self->start_args[9] = g_strdup("--listen");
    self->start_args[10] = g_strdup(evp_socket);
    self->start_args[11] = g_strdup("--no-service-discovery");
    if ( ! enable_relay )
        self->start_args[12] = g_strdup("--no-relay");

    self->stop_args = g_new0(char *, 5);
    self->stop_args[0] = g_strdup(self->start_args[0]);
    self->stop_args[1] = g_strdup("--socket");
    self->stop_args[2] = g_strdup(socket);
    self->stop_args[3] = g_strdup("stop");


    return self;
}

void
eventd_tests_env_free(EventdTestsEnv *self)
{
    g_strfreev(self->stop_args);
    g_strfreev(self->start_args);
    g_strfreev(self->env);
    g_free(self);
}

gboolean
eventd_tests_env_start_eventd(EventdTestsEnv *self)
{
    GError *error = NULL;
    if ( g_spawn_sync(self->dir, self->start_args, self->env, 0, NULL, NULL, NULL, NULL, NULL, &error) )
        return TRUE;
    g_warning("Failed to start eventd: %s", error->message);
    g_error_free(error);
    return FALSE;
}

gboolean
eventd_tests_env_stop_eventd(EventdTestsEnv *self)
{
    GError *error = NULL;
    if ( g_spawn_sync(self->dir, self->stop_args, self->env, 0, NULL, NULL, NULL, NULL, NULL, &error) )
        return TRUE;
    g_warning("Failed to stop eventd: %s", error->message);
    g_error_free(error);
    return FALSE;
}
