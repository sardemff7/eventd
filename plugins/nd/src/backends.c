/*
 * eventd - Small daemon to act on remote or local events
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

#include "config.h"

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>

#include <cairo.h>

#include "libeventd-helpers-dirs.h"

#include "backend.h"
#include "backends.h"

static gboolean
_eventd_nd_backends_load_dir(EventdNdBackend *backends, EventdNdInterface *context, NkBindings *bindings, gchar *backends_dir_name)
{
    gboolean ret = FALSE;

    eventd_debug("Scanning notification backends dir: %s", backends_dir_name);

    EventdNdBackends i;
    for ( i = EVENTD_ND_BACKEND_NONE + 1 ; i < _EVENTD_ND_BACKENDS_SIZE ; ++i )
    {
        if ( backends[i].context != NULL )
            continue;

        gchar name[12]; /* wayland.dll + \0 */
        gchar *file;
        g_snprintf(name, sizeof(name), "%s." G_MODULE_SUFFIX, eventd_nd_backends_names[i]);
        file = g_build_filename(backends_dir_name, name, NULL);

        if ( ( ! g_file_test(file, G_FILE_TEST_EXISTS) ) || g_file_test(file, G_FILE_TEST_IS_DIR) )
            goto next;

        GModule *module;
        module = g_module_open(file, G_MODULE_BIND_LAZY|G_MODULE_BIND_LOCAL);
        if ( module == NULL )
        {
            g_warning("Couldn't load module '%s': %s", eventd_nd_backends_names[i], g_module_error());
            goto next;
        }

        EventdNdBackendGetInfoFunc get_info;
        if ( ! g_module_symbol(module, "eventd_nd_backend_get_info", (void **)&get_info) )
            goto next;

        eventd_debug("Loading backend '%s'", file);

        EventdNdBackend backend = { .module = NULL, .context = NULL };
        get_info(&backend);
        backend.name = eventd_nd_backends_names[i];
        backend.module = module;
        backend.context = backend.init(context, bindings);

        if ( backend.context != NULL )
        {
            backends[i] = backend;
            ret = TRUE;
        }
        else
            g_module_close(module);

    next:
        g_free(file);
    }
    g_free(backends_dir_name);

    return ret;
}

gboolean
eventd_nd_backends_load(EventdNdBackend *backends, EventdNdInterface *context, NkBindings *bindings)
{
    gboolean ret = FALSE;

    gchar **dirs, **dir;
    dirs = evhelpers_dirs_get_lib("NOTIFICATION_BACKENDS", "modules" G_DIR_SEPARATOR_S MODULES_VERSION G_DIR_SEPARATOR_S "nd");
    for ( dir = dirs ; *dir != NULL ; ++dir )
        ret = _eventd_nd_backends_load_dir(backends, context, bindings, *dir) || ret;
    g_free(dirs);

    return ret;
}

void
eventd_nd_backends_unload(EventdNdBackend *backends)
{
    EventdNdBackends i;
    for ( i = EVENTD_ND_BACKEND_NONE + 1 ; i < _EVENTD_ND_BACKENDS_SIZE ; ++i )
    {
        if ( backends[i].context == NULL )
            continue;

        backends[i].uninit(backends[i].context);
        g_module_close(backends[i].module);
    }
}
