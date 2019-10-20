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

#ifndef __EVENTD_PLUGINS_H__
#define __EVENTD_PLUGINS_H__

void eventd_plugins_load(EventdPluginCoreContext *core, const gchar * const *evp_binds, gboolean enable_relay, gboolean enable_sd_modules, gboolean system_mode);
void eventd_plugins_unload(void);

void eventd_plugins_start_all(void);
void eventd_plugins_stop_all(void);

EventdctlReturnCode eventd_plugins_control_command(const gchar *id, guint64 argc, const gchar * const *argv, gchar **status);

void eventd_plugins_config_init_all(void);
void eventd_plugins_config_reset_all(void);
void eventd_plugins_relay_set_certificate(GTlsCertificate *certificate);

void eventd_plugins_global_parse_all(GKeyFile *config_file);
GList *eventd_plugins_event_parse_all(GKeyFile *config_file);

void eventd_plugins_event_dispatch_all(EventdEvent *event);
void eventd_plugins_event_action_all(const GList *actions, EventdEvent *event);

void eventd_plugins_action_free(gpointer data);
#endif /* __EVENTD_PLUGINS_H__ */
