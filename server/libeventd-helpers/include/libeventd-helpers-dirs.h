/*
 * libeventd-helpers - Internal helpers
 *
 * Copyright © 2011-2015 Quentin "Sardem FF7" Glidic
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

#ifndef __LIBEVENTD_HELPERS_DIRS_H__
#define __LIBEVENTD_HELPERS_DIRS_H__

#include <libeventd-event.h>

gchar **evhelpers_dirs_get_config(const gchar *env, const gchar *subdir);
gchar **evhelpers_dirs_get_lib(const gchar *env, const gchar *subdir);

#endif /* __LIBEVENTD_HELPERS_DIRS_H__ */
