/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2024 Morgane "Sardem FF7" Glidic
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

#include <locale.h>

#include <glib.h>

void eventd_tests_add_events_suite(void);

int
main(int argc, char *argv[])
{
    setlocale(LC_ALL, "C");

    g_setenv("LANG", "C", TRUE);
    g_setenv("TZ", "UTC", TRUE);

    g_test_init(&argc, &argv, NULL);

    g_test_set_nonfatal_assertions();

    eventd_tests_add_events_suite();

    return g_test_run();
}
