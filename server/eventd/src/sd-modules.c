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

#include "config.h"

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>
#include <gio/gio.h>


#include "libeventd-helpers-dirs.h"

#include "eventd-sd-module.h"
#include "sd-modules.h"

static const gchar *_eventd_sd_modules_names[_EVENTD_SD_MODULES_SIZE] = {
    [EVENTD_SD_MODULE_DNS_SD] = "dns-sd." G_MODULE_SUFFIX,
    [EVENTD_SD_MODULE_SSDP] = "ssdp." G_MODULE_SUFFIX,
};

static EventdSdModule modules[_EVENTD_SD_MODULES_SIZE];

static void
_eventd_sd_modules_load_dir(const EventdSdModuleControlInterface *control, GList *sockets, gchar *modules_dir_name)
{
    eventd_debug("Scanning service discovery modules dir: %s", modules_dir_name);

    EventdSdModules i;
    for ( i = EVENTD_SD_MODULE_NONE + 1 ; i < _EVENTD_SD_MODULES_SIZE ; ++i )
    {
        if ( modules[i].context != NULL )
            continue;

        gchar *file;
        file = g_build_filename(modules_dir_name, _eventd_sd_modules_names[i], NULL);

        if ( ( ! g_file_test(file, G_FILE_TEST_EXISTS) ) || g_file_test(file, G_FILE_TEST_IS_DIR) )
            goto next;

        GModule *module;
        module = g_module_open(file, G_MODULE_BIND_LAZY|G_MODULE_BIND_LOCAL);
        if ( module == NULL )
        {
            g_warning("Couldn't load module '%s': %s", _eventd_sd_modules_names[i], g_module_error());
            goto next;
        }

        EventdSdModuleGetInfoFunc get_info;
        if ( ! g_module_symbol(module, "eventd_sd_module_get_info", (void **)&get_info) )
            goto next;

        eventd_debug("Loading service discovery module '%s'", file);

        EventdSdModule sd_module = { .module = NULL, .context = NULL };
        get_info(&sd_module);
        sd_module.module = module;
        sd_module.context = sd_module.init(control, sockets);

        if ( sd_module.context != NULL )
            modules[i] = sd_module;
        else
            g_module_close(module);

    next:
        g_free(file);
    }
    g_free(modules_dir_name);
}

void
eventd_sd_modules_load(const EventdSdModuleControlInterface *control, GList *sockets)
{
    gchar **dirs, **dir;
    dirs = evhelpers_dirs_get_lib("MODULES", "modules" G_DIR_SEPARATOR_S MODULES_VERSION);
    for ( dir = dirs ; *dir != NULL ; ++dir )
        _eventd_sd_modules_load_dir(control, sockets, *dir);
    g_free(dirs);
}

#define _eventd_sd_modules_foreach(code) G_STMT_START { \
        EventdSdModules i; \
        for ( i = EVENTD_SD_MODULE_NONE + 1 ; i < _EVENTD_SD_MODULES_SIZE ; ++i ) \
        { \
            if ( modules[i].context == NULL ) \
                continue; \
            \
            code; \
        } \
    } G_STMT_END

void
eventd_sd_modules_unload(void)
{
    EventdSdModules i;
    for ( i = EVENTD_SD_MODULE_NONE + 1 ; i < _EVENTD_SD_MODULES_SIZE ; ++i )
    {
        if ( modules[i].context == NULL )
            continue;

        modules[i].uninit(modules[i].context);
        g_module_close(modules[i].module);
    }
}

#define EV_COMMA ,
#define _eventd_sd_modules_foreach_call(name, args) _eventd_sd_modules_foreach(modules[i].name(modules[i].context args))

void
eventd_sd_modules_set_publish_name(const gchar *name)
{
    _eventd_sd_modules_foreach_call(set_publish_name, EV_COMMA name);
}

void
eventd_sd_modules_monitor_server(const gchar *name, EventdRelayServer *server)
{
    _eventd_sd_modules_foreach_call(monitor_server, EV_COMMA name EV_COMMA server);
}

void
eventd_sd_modules_start(void)
{
    _eventd_sd_modules_foreach_call(start,);
}

void
eventd_sd_modules_stop(void)
{
    _eventd_sd_modules_foreach_call(stop,);
}

gboolean
eventd_sd_modules_can_discover(void)
{
    gboolean ret = FALSE;
    _eventd_sd_modules_foreach(ret = TRUE);
    return ret;
}
