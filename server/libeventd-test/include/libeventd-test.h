/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2016 Quentin "Sardem FF7" Glidic
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

#ifndef __EVENTD_LIBEVENTD_TEST_H__
#define __EVENTD_LIBEVENTD_TEST_H__

#include <glib.h>

typedef struct _EventdTestsEnv EventdTestsEnv;

void eventd_tests_env_setup(gchar **argv);
EventdTestsEnv *eventd_tests_env_new(const gchar *plugins, gchar **argv, gint argc);
void eventd_tests_env_free(EventdTestsEnv *env);
gboolean eventd_tests_env_start_eventd(EventdTestsEnv *env);
gboolean eventd_tests_env_stop_eventd(EventdTestsEnv *env);

int eventd_tests_run_libeventc(const gchar *host);

#endif /* __EVENTD_LIBEVENTD_TEST_H__ */
