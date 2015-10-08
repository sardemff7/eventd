/*
 * eventd - Small daemon to act on remote or local events
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

#ifndef __EVENTD_FDO_NOTIFICATIONS_H__
#define __EVENTD_FDO_NOTIFICATIONS_H__

struct _EventdPluginContext {
    EventdPluginCoreContext *core;
    EventdPluginCoreInterface *core_interface;
    GDBusNodeInfo *introspection_data;
    guint id;
    gboolean bus_name_owned;
    GDBusConnection *connection;
    GVariant *capabilities;
    GVariant *server_information;
    guint32 count;
    GHashTable *events;
    GHashTable *notifications;
    struct {
        guint id;
        GDBusProxy *server;
        GSList *actions;
        gboolean overlay_icon;
    } client;
};

void eventd_libnotify_get_interface(EventdPluginInterface *interface);
void eventd_libnotify_start(EventdPluginContext *context);
void eventd_libnotify_stop(EventdPluginContext *context);


#define NOTIFICATION_BUS_NAME      "org.freedesktop.Notifications"
#define NOTIFICATION_BUS_PATH      "/org/freedesktop/Notifications"

#define NOTIFICATION_SPEC_VERSION  "1.2"

#endif /* __EVENTD_FDO_NOTIFICATIONS_H__ */
