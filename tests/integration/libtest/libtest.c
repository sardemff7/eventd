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

#include <glib.h>

#include <libeventd-test.h>

#define TESTS_DIR      BUILD_DIR G_DIR_SEPARATOR_S "tests"
#define PLUGINS_DIR    BUILD_DIR G_DIR_SEPARATOR_S LT_OBJDIR

#define EVENTD_PATH    BUILD_DIR G_DIR_SEPARATOR_S "eventd" EXEEXT
#define EVENTDCTL_PATH BUILD_DIR G_DIR_SEPARATOR_S "eventdctl" EXEEXT

struct _EventdTestsEnv {
    gchar **env;
    gchar **start_args;
    gchar **stop_args;
};

void
eventd_tests_env_setup()
{
    g_setenv("EVENTD_CONFIG_DIR", TESTS_DIR, TRUE);
    g_setenv("XDG_RUNTIME_DIR", TESTS_DIR, TRUE);
    g_setenv("EVENTD_PLUGINS_DIR", PLUGINS_DIR, TRUE);
}

EventdTestsEnv *
eventd_tests_env_new(const gchar *plugins, const gchar *port, gchar **argv, gint argc)
{
    EventdTestsEnv *self;
    gint i;

    self = g_new0(EventdTestsEnv, 1);


    self->env = g_get_environ();
    self->env = g_environ_setenv(self->env, "EVENTD_PLUGINS_WHITELIST", plugins, TRUE);


    self->start_args = g_new(char *, 10+argc);
    self->start_args[0] = g_strdup(EVENTDCTL_PATH);
    self->start_args[1] = g_strdup("--socket");
    self->start_args[2] = g_strdup(port);
    self->start_args[3] = g_strdup("start");
    self->start_args[4] = g_strdup("--argv0");
    self->start_args[5] = g_strdup(EVENTD_PATH);
    self->start_args[6] = g_strdup("--take-over");
    self->start_args[7] = g_strdup("--private-socket");
    self->start_args[8] = g_strdup(port);
    for ( i = 0 ; i < argc ; ++i )
        self->start_args[9+i] = argv[i];
    self->start_args[9+i] = NULL;
    g_free(argv);


    self->stop_args = g_new(char *, 5);
    self->stop_args[0] = g_strdup(EVENTDCTL_PATH);
    self->stop_args[1] = g_strdup("--socket");
    self->stop_args[2] = g_strdup(port);
    self->stop_args[3] = g_strdup("quit");
    self->stop_args[4] = NULL;


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

void
eventd_tests_env_start_eventd(EventdTestsEnv *self, GError **error)
{
    g_spawn_sync(BUILD_DIR, self->start_args, self->env, 0, NULL, NULL, NULL, NULL, NULL, error);
}

void
eventd_tests_env_stop_eventd(EventdTestsEnv *self, GError **error)
{
    g_spawn_sync(BUILD_DIR, self->start_args, self->env, 0, NULL, NULL, NULL, NULL, NULL, error);
}
