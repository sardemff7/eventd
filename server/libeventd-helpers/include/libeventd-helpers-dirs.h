/*
 * libeventd-helpers - Internal helpers
 *
 * Copyright © 2011-2021 Quentin "Sardem FF7" Glidic
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

#include <string.h>

#include <glib.h>

static inline gchar **
_evhelpers_dirs_get_env_path(const gchar *env_path, const gchar *env_dir, gsize *size)
{
    gchar **env_paths = NULL;
    *size = 0;

    env_path = g_getenv(env_path);
    if ( env_path == NULL )
    {
        env_dir = g_getenv(env_dir);
        if ( env_dir == NULL )
            return NULL;

        env_paths = g_new(gchar *, 1);
        env_paths[0] = g_strdup(env_dir);
        *size = 1;
    }
    else
    {
        env_paths = g_strsplit(env_path, G_SEARCHPATH_SEPARATOR_S, -1);
        *size = g_strv_length(env_paths);
    }

    gsize i;
    for ( i = 0 ; i < *size ; ++i )
    {
        gchar *path = env_paths[i];
        if ( g_str_has_prefix(path, "~/") )
        {
            env_paths[i] = g_build_filename(g_get_home_dir(), path + strlen("~/"), NULL);
            g_free(path);
        }
    }

    return env_paths;
}

static inline gsize
_evhelpers_dirs_add(gchar **dirs, const gchar * const *list, gsize size, gboolean env, gboolean reversed, const gchar *subdir)
{
    gsize i, j = 0;

    for ( i = 0 ; i < size ; ++i )
    {
        const gchar *base_dir;
        base_dir = list[reversed ? ( size - i -1 ) : i];
        if ( base_dir == NULL )
            continue;

        gchar *dir;
        dir = env ? ( (gchar *) base_dir ) : g_build_filename(base_dir, PACKAGE_NAME, subdir, NULL);

        if ( g_file_test(dir, G_FILE_TEST_IS_DIR) )
            dirs[j++] = dir;
        else
            g_free(dir);
    }

    return j;
}

static inline gchar **
evhelpers_dirs_get_config(const gchar *env_path, const gchar *env_dir, gboolean system_mode, const gchar *subdir)
{
    const gchar *datadir = EVENTD_DATADIR;
    const gchar *confdir = EVENTD_SYSCONFDIR;
    const gchar * const *glib_data_paths = g_get_system_data_dirs();

#ifdef G_OS_WIN32
    gchar *sysconfdir_, *installdir;

    installdir = g_win32_get_package_installation_directory_of_module(NULL);
    sysconfdir_ = g_build_filename(installdir, "etc", NULL);
    g_free(installdir);

    datadir = NULL;
    confdir = sysconfdir_;
#else /* ! G_OS_WIN32 */
    if ( g_strv_contains(glib_data_paths, datadir) )
        datadir = NULL;
#endif /* ! G_OS_WIN32 */

    if ( ! system_mode )
        confdir = g_get_user_config_dir();

    const gchar *dirs_[] = { datadir, confdir };

    gsize env_size;
    gchar **env_paths;
    env_paths = _evhelpers_dirs_get_env_path(env_path, env_dir, &env_size);

    gsize glib_data_size = g_strv_length((gchar **) glib_data_paths);

    gsize o = 0;
    gchar **dirs;
    dirs = g_new(gchar *, G_N_ELEMENTS(dirs_) + glib_data_size + env_size + 1);
    o += _evhelpers_dirs_add(dirs + o, glib_data_paths, glib_data_size, FALSE, TRUE, subdir);
    o += _evhelpers_dirs_add(dirs + o, dirs_, G_N_ELEMENTS(dirs_), FALSE, FALSE, subdir);
    o += _evhelpers_dirs_add(dirs + o, (const gchar * const *) env_paths, env_size, TRUE, FALSE, NULL);
    dirs[o] = NULL;

    g_free(env_paths);
#ifdef G_OS_WIN32
    g_free(sysconfdir_);
#endif /* G_OS_WIN32 */

    return dirs;
}
#define evhelpers_dirs_get_config(env, system_mode, subdir) evhelpers_dirs_get_config("EVENTD_" env "_PATH", "EVENTD_" env "_DIR", system_mode, subdir)

static inline gchar **
evhelpers_dirs_get_lib(const gchar *env_path, const gchar *env_dir, const gchar *subdir)
{
    const gchar *libdir = EVENTD_LIBDIR;

#ifdef G_OS_WIN32
    gchar *libdir_, *installdir;

    installdir = g_win32_get_package_installation_directory_of_module(NULL);
    libdir_ = g_build_filename(installdir, "lib", NULL);
    g_free(installdir);

    libdir = libdir_;
#endif /* G_OS_WIN32 */

    const gchar *dirs_[] = { g_get_user_data_dir(), libdir };

    gsize size;
    gchar **env_paths;
    env_paths = _evhelpers_dirs_get_env_path(env_path, env_dir, &size);

    gsize o = 0;
    gchar **dirs;
    dirs = g_new(gchar *, G_N_ELEMENTS(dirs_) + size + 1);
    o += _evhelpers_dirs_add(dirs + o, (const gchar * const *) env_paths, size, TRUE, FALSE, NULL);
    o += _evhelpers_dirs_add(dirs + o, dirs_, G_N_ELEMENTS(dirs_), FALSE, FALSE, subdir);
    dirs[o] = NULL;

    g_free(env_paths);
#ifdef G_OS_WIN32
    g_free(libdir_);
#endif /* G_OS_WIN32 */

    return dirs;
}
#define evhelpers_dirs_get_lib(env, subdir) evhelpers_dirs_get_lib("EVENTD_" env "_PATH", "EVENTD_" env "_DIR", subdir)

#endif /* __LIBEVENTD_HELPERS_DIRS_H__ */
