/*
 * libeventd-helpers - Internal helpers
 *
 * Copyright © 2011-2017 Quentin "Sardem FF7" Glidic
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <glib.h>

static inline gchar **
_evhelpers_dirs_get(const gchar **list, gsize size, const gchar *env, const gchar *subdir)
{
    gchar **dirs;
    gsize i, j = 0;

    dirs = g_new0(gchar *, size + 1);
    for ( i = 0 ; i < size ; ++i )
    {
        gchar *dir;
        if ( list[i] == NULL )
            continue;

        if ( list[i] == env )
        {
            env = g_getenv(env);
            if ( env == NULL )
                continue;

            if ( g_str_has_prefix(env, "~/") )
                dir = g_build_filename(g_get_home_dir(), env + strlen("~/"), NULL);
            else
                dir = g_strdup(env);
        }
        else
            dir = g_build_filename(list[i], PACKAGE_NAME, subdir, NULL);

        if ( g_file_test(dir, G_FILE_TEST_IS_DIR) )
            dirs[j++] = dir;
        else
            g_free(dir);
    }

    return dirs;
}

static inline gchar **
evhelpers_dirs_get_config(const gchar *env, gboolean system_mode, const gchar *subdir)
{
    const gchar *datadir = EVENTD_DATADIR;
    const gchar *confdir = EVENTD_SYSCONFDIR;

#ifdef G_OS_WIN32
    gchar *datadir_, *sysconfdir_, *installdir;

    installdir = g_win32_get_package_installation_directory_of_module(NULL);
    datadir_ = g_build_filename(installdir, "share", NULL);
    sysconfdir_ = g_build_filename(installdir, "etc", NULL);
    g_free(installdir);

    datadir = datadir_;
    confdir = sysconfdir_;
#endif /* G_OS_WIN32 */

    if ( ! system_mode )
        confdir = g_get_user_config_dir();

    const gchar *dirs_[] = { datadir, confdir, env };
    gchar **dirs;

    dirs = _evhelpers_dirs_get(dirs_, G_N_ELEMENTS(dirs_), env, subdir);

#ifdef G_OS_WIN32
    g_free(datadir_);
    g_free(sysconfdir_);
#endif /* G_OS_WIN32 */

    return dirs;
}

static inline gchar **
evhelpers_dirs_get_lib(const gchar *env, const gchar *subdir)
{
    const gchar *libdir = EVENTD_LIBDIR;

#ifdef G_OS_WIN32
    gchar *libdir_, *installdir;

    installdir = g_win32_get_package_installation_directory_of_module(NULL);
    libdir_ = g_build_filename(installdir, "lib", NULL);
    g_free(installdir);

    libdir = libdir_;
#endif /* G_OS_WIN32 */

    const gchar *dirs_[] = { env, g_get_user_data_dir(), libdir, NULL };
    gchar **dirs;

    dirs = _evhelpers_dirs_get(dirs_, G_N_ELEMENTS(dirs_), env, subdir);

#ifdef G_OS_WIN32
    g_free(libdir_);
#endif /* G_OS_WIN32 */

    return dirs;
}

#endif /* __LIBEVENTD_HELPERS_DIRS_H__ */
