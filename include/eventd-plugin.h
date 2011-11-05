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

#ifndef __EVENTD_EVENTD_PLUGIN_H__
#define __EVENTD_EVENTD_PLUGIN_H__

typedef struct {
	gchar *type;
	gchar *name;
} EventdClient;

typedef struct {
	EventdClient *client;

	gchar *type;
	GHashTable *data;
} EventdEvent;

typedef void (*EventdPluginStartFunc)(gpointer user_data);
typedef void (*EventdPluginStopFunc)(void);
typedef void (*EventdPluginConfigInitFunc)(void);
typedef void (*EventdPluginConfigCleanFunc)(void);
typedef void (*EventdPluginEventParseFunc)(const gchar *, const gchar *, GKeyFile *);
typedef void (*EventdPluginEventActionFunc)(EventdEvent *event);

typedef struct {
	EventdPluginStartFunc start;
	EventdPluginStopFunc stop;

	EventdPluginConfigInitFunc config_init;
	EventdPluginConfigCleanFunc config_clean;

	EventdPluginEventParseFunc event_parse;
	EventdPluginEventActionFunc event_action;

	/* Private stuff */
	void *module;
} EventdPlugin;

typedef EventdPlugin *(*EventdPluginGetInfoFunc)(EventdPlugin *plugin);

#endif /* __EVENTD_EVENTD_PLUGIN_H__ */
