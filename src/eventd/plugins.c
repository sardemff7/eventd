/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011 Quentin "Sardem FF7" Glidic
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
#include <gmodule.h>

#include "eventd-plugin.h"
#include "plugins.h"

static GList *plugins = NULL;

void
eventd_plugins_load()
{
	GError *error;
	GDir *plugins_dir;
	gchar *file;

	if ( ! g_module_supported() )
	{
		g_warning("Couldn’t load plugins: %s", g_module_error());
		return;
	}

	plugins_dir = g_dir_open(PLUGINS_DIR, 0, &error);
	if ( ! plugins_dir )
	{
		g_warning("Can’t read the plugins directory: %s", error->message);
		g_clear_error(&error);
		return;
	}

	while ( ( file = (gchar *)g_dir_read_name(plugins_dir) ) != NULL )
	{
		gchar *full_filename;
		EventdPlugin *plugin;
		EventdPluginGetInfoFunc get_info;
		void *module;

		full_filename = g_strdup_printf(PLUGINS_DIR "/%s", file);
		module = g_module_open(full_filename, G_MODULE_BIND_LAZY|G_MODULE_BIND_LOCAL);
		if ( module == NULL )
		{
			g_warning("Couldn’t load module '%s': %s", file, g_module_error());
			goto next;
		}

		if ( ! g_module_symbol(module, "eventd_plugin_get_info", (void **)&get_info) )
			goto next;

		plugin = g_new0(EventdPlugin, 1);
		plugin->module = module;
		get_info(plugin);

		if ( plugin->start != NULL )
			plugin->start();

		plugins = g_list_prepend(plugins, plugin);

	next:
		g_free(full_filename);
	}
}

static void
plugin_free(EventdPlugin *plugin)
{
	if ( plugin->stop != NULL )
		plugin->stop();
	g_module_close(plugin->module);
	g_free(plugin);
}

void
eventd_plugins_unload()
{
	if ( plugins == NULL )
		return;
	g_list_free_full(plugins, (GDestroyNotify)plugin_free);
}

void
eventd_plugins_foreach(GFunc func, gpointer user_data)
{
	if ( plugins == NULL )
		return;
	g_list_foreach(plugins, func, user_data);
}
