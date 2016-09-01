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

#include "config.h"

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>
#include <gio/gio.h>


#include <libeventd-helpers-dirs.h>

#include <eventd-ws-module.h>

static EventdWsModule *
_eventd_ws_module_load_dir(gchar *modules_dir_name)
{
#ifdef EVENTD_DEBUG
    g_debug("Scanning modules dir: %s", modules_dir_name);
#endif /* EVENTD_DEBUG */

    gchar *file;
    file = g_build_filename(modules_dir_name, "ws." G_MODULE_SUFFIX, NULL);
    g_free(modules_dir_name);

    if ( ( ! g_file_test(file, G_FILE_TEST_EXISTS) ) || g_file_test(file, G_FILE_TEST_IS_DIR) )
    {
        g_free(file);
        return NULL;
    }

    GModule *module;
    module = g_module_open(file, G_MODULE_BIND_LAZY|G_MODULE_BIND_LOCAL);
    g_free(file);
    if ( module == NULL )
    {
        g_warning("Couldn't load WebSocket module: %s", g_module_error());
        return NULL;
    }

    EventdWsModuleGetInfoFunc get_info;
    if ( ! g_module_symbol(module, "eventd_ws_module_get_info", (void **)&get_info) )
        return NULL;

#ifdef EVENTD_DEBUG
    g_debug("Loading WebSocket module");
#endif /* ! EVENTD_DEBUG */

    EventdWsModule *ws_module;
    ws_module = g_new(EventdWsModule, 1);
    get_info(ws_module);
    ws_module->module = module;
    return ws_module;
}

EventdWsModule *
eventd_ws_init(void)
{
    if ( ! g_module_supported() )
        return NULL;

    EventdWsModule *ws = NULL;
    gchar **dirs, **dir;
    dirs = evhelpers_dirs_get_lib("EVENTD_MODULES_DIR", "modules" G_DIR_SEPARATOR_S PACKAGE_VERSION);
    for ( dir = dirs ; ( ws == NULL ) && ( *dir != NULL ) ; ++dir )
        ws = _eventd_ws_module_load_dir(*dir);
    g_free(dirs);
    return ws;
}

void
eventd_ws_uninit(EventdWsModule *module)
{
    if ( module == NULL )
        return;

    g_module_close(module->module);
    g_free(module);
}

