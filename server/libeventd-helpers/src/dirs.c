/*
 * libeventd-helpers - Internal helpers
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

#include <config.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <glib.h>

#include <libeventd-helpers-dirs.h>

static gsize
_evhelpers_dirs_add(gchar **dirs, gsize i, gchar *path)
{
    if ( g_file_test(path, G_FILE_TEST_IS_DIR) )
        dirs[i++] = path;
    else
        g_free(path);
    return i;
}

static gsize
_evhelpers_dirs_add_env(gchar **dirs, gsize i, const gchar *env)
{
    if ( env == NULL )
        return i;
    env = g_getenv(env);
    if ( env == NULL )
        return i;
    if ( g_str_has_prefix(env, "~/") )
        i = _evhelpers_dirs_add(dirs, i, g_build_filename(g_get_home_dir(), env + strlen("~/"), NULL));
    else if ( g_file_test(env, G_FILE_TEST_IS_DIR) )
        dirs[i++] = g_strdup(env);
    return i;
}

EVENTD_EXPORT
gchar **
evhelpers_dirs_get_config(const gchar *env, const gchar *subdir)
{
    const gchar *datadir = DATADIR;
    const gchar *sysconfdir = SYSCONFDIR;

#ifdef G_OS_WIN32
    gchar *datadir_, *sysconfdir_, *installdir;

    installdir = g_win32_get_package_installation_directory_of_module(NULL);
    datadir_ = g_build_filename(installdir, "share", NULL);
    sysconfdir_ = g_build_filename(installdir, "etc", NULL);
    g_free(installdir);

    datadir = datadir_;
    sysconfdir = sysconfdir_;
#endif /* G_OS_WIN32 */

    gsize i = 0;
    gchar **dirs;

    dirs = g_new0(gchar *, 5);

    const gchar *dirs_[] = { datadir, sysconfdir, g_get_user_config_dir(), NULL };
    const gchar * const *dir;

    for ( dir = dirs_ ; *dir != NULL ; ++dir )
        i = _evhelpers_dirs_add(dirs, i, g_build_filename(*dir, PACKAGE_NAME, subdir, NULL));
    _evhelpers_dirs_add_env(dirs, i, env);

#ifdef G_OS_WIN32
    g_free(datadir_);
    g_free(sysconfdir_);
#endif /* G_OS_WIN32 */

    return dirs;
}

EVENTD_EXPORT
gchar **
evhelpers_dirs_get_lib(const gchar *env, const gchar *subdir)
{
    const gchar *libdir = LIBDIR;

#ifdef G_OS_WIN32
    gchar *libdir_, *installdir;

    installdir = g_win32_get_package_installation_directory_of_module(NULL);
    libdir_ = g_build_filename(installdir, "lib", NULL);
    g_free(installdir);

    libdir = libdir_;
#endif /* G_OS_WIN32 */

    gsize i = 0;
    gchar **dirs;

    dirs = g_new0(gchar *, 4);

    const gchar *dirs_[] = { g_get_user_data_dir(), libdir, NULL };
    const gchar * const *dir;

    i = _evhelpers_dirs_add_env(dirs, i, env);
    for ( dir = dirs_ ; *dir != NULL ; ++dir )
        i = _evhelpers_dirs_add(dirs, i, g_build_filename(*dir, PACKAGE_NAME, subdir, NULL));

#ifdef G_OS_WIN32
    g_free(libdir_);
#endif /* G_OS_WIN32 */

    return dirs;
}
