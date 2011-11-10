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

#include <eventd-plugin.h>
#include <config-helper.h>
#include <plugin-helper.h>

#include "notification.h"
#include "icon.h"
#include "notify.h"


static GHashTable *events = NULL;

static void
eventd_notification_start(gpointer user_data)
{
    eventd_notification_notify_init();

    eventd_plugin_helper_regex_init();
}

static void
eventd_notification_stop()
{
    eventd_plugin_helper_regex_clean();

    eventd_notification_notify_uninit();
}

static void
eventd_notification_event_update(EventdNotificationEvent *event, gboolean disable, const char *title, const char *message, const char *icon, const char *overlay_icon, Int *scale, Int *timeout)
{
    event->disable = disable;
    if ( title != NULL )
    {
        g_free(event->title);
        event->title = g_strdup(title);
    }
    if ( message != NULL )
    {
        g_free(event->message);
        event->message = g_strdup(message);
    }
    if ( icon != NULL )
    {
        g_free(event->icon);
        event->icon = g_strdup(icon);
    }
    if ( overlay_icon != NULL )
    {
        g_free(event->overlay_icon);
        event->overlay_icon = g_strdup(overlay_icon);
    }
    if ( scale->set )
        event->scale = (gdouble)scale->value / 100.;
    if ( timeout->set )
        event->timeout = ( timeout->value > 0 ) ? ( timeout->value * 1000 ) : timeout->value;
}

static EventdNotificationEvent *
eventd_notification_event_new(gboolean disable, const char *title, const char *message, const char *icon, const char *overlay_icon, Int *scale, Int *timeout, EventdNotificationEvent *parent)
{
    EventdNotificationEvent *event = NULL;

    title = title ?: parent ? parent->title : "$client-name - $event-data[name]";
    message = message ?: parent ? parent->message : "$event-data[text]";
    icon = icon ?: parent ? parent->icon : "icon";
    overlay_icon = overlay_icon ?: parent ? parent->overlay_icon : "overlay-icon";
    scale->value = scale->set ? scale->value : parent ? parent->scale * 100 : 50;
    scale->set = TRUE;
    timeout->value = timeout->set ? timeout->value : parent ? parent->timeout : -1;
    timeout->set = TRUE;

    event = g_new0(EventdNotificationEvent, 1);

    eventd_notification_event_update(event, disable, title, message, icon, overlay_icon, scale, timeout);

    return event;
}

static void
eventd_notification_event_free(EventdNotificationEvent *event)
{
    g_free(event->icon);
    g_free(event->overlay_icon);
    g_free(event->message);
    g_free(event->title);
    g_free(event);
}

static void
eventd_notification_event_parse(const gchar *client_type, const gchar *event_type, GKeyFile *config_file)
{
    gboolean disable;
    gchar *name = NULL;
    gchar *title = NULL;
    gchar *message = NULL;
    gchar *icon = NULL;
    gchar *overlay_icon = NULL;
    Int scale;
    Int timeout;
    EventdNotificationEvent *event;

    if ( ! g_key_file_has_group(config_file, "notification") )
        return;

    if ( eventd_config_helper_key_file_get_boolean(config_file, "notification", "disable", &disable) < 0 )
        goto skip;
    if ( eventd_config_helper_key_file_get_string(config_file, "notification", "title", &title) < 0 )
        goto skip;
    if ( eventd_config_helper_key_file_get_string(config_file, "notification", "message", &message) < 0 )
        goto skip;
    if ( eventd_config_helper_key_file_get_string(config_file, "notification", "icon", &icon) < 0 )
        goto skip;
    if ( eventd_config_helper_key_file_get_string(config_file, "notification", "overlay-icon", &overlay_icon) < 0 )
        goto skip;
    if ( eventd_config_helper_key_file_get_int(config_file, "notification", "overlay-scale", &scale) < 0 )
        goto skip;
    if ( eventd_config_helper_key_file_get_int(config_file, "notification", "timeout", &timeout) < 0 )
        goto skip;

    if ( event_type != NULL )
        name = g_strconcat(client_type, "-", event_type, NULL);
    else
        name = g_strdup(client_type);

    event = g_hash_table_lookup(events, name);
    if ( event != NULL )
        eventd_notification_event_update(event, disable, title, message, icon, overlay_icon, &scale, &timeout);
    else
        g_hash_table_insert(events, name, eventd_notification_event_new(disable, title, message, icon, overlay_icon, &scale, &timeout, g_hash_table_lookup(events, client_type)));

skip:
    g_free(overlay_icon);
    g_free(icon);
    g_free(message);
    g_free(title);
}

static EventdNotificationNotification *
eventd_notification_notification_new(EventdEvent *event, EventdNotificationEvent *notification_event)
{
    EventdNotificationNotification *notification;
    gchar *tmp = NULL;

    notification = g_new0(EventdNotificationNotification, 1);

    tmp = eventd_plugin_helper_regex_replace_client_name(notification_event->title, event->client->name);
    notification->title = eventd_plugin_helper_regex_replace_event_data(tmp, event->data, NULL);
    g_free(tmp);

    notification->message = eventd_plugin_helper_regex_replace_event_data(notification_event->message, event->data, NULL);

    eventd_notification_icon_get_pixbuf(event, notification_event, notification);

    notification->timeout = notification_event->timeout;

    return notification;
}


static void
eventd_notification_notification_insert_data_in_hash_table(EventdNotificationNotification *notification, GHashTable *table)
{
    g_hash_table_insert(table, g_strdup("title"), g_strdup(notification->title));
    g_hash_table_insert(table, g_strdup("message"), g_strdup(notification->message));

    if ( notification->icon != NULL )
        g_hash_table_insert(table, g_strdup("icon"), eventd_notification_icon_get_base64(notification->icon));
    if ( notification->overlay_icon != NULL )
        g_hash_table_insert(table, g_strdup("overlay-icon"), eventd_notification_icon_get_base64(notification->overlay_icon));
    if ( notification->merged_icon != NULL )
        g_hash_table_insert(table, g_strdup("merged-icon"), eventd_notification_icon_get_base64(notification->merged_icon));

    g_hash_table_insert(table, g_strdup("timeout"), g_strdup_printf("%jd", notification->timeout));
}

static void
eventd_notification_notification_free(EventdNotificationNotification *notification)
{
    if ( notification->merged_icon != NULL )
        eventd_notification_icon_unref(notification->merged_icon);
    if ( notification->overlay_icon != NULL )
        eventd_notification_icon_unref(notification->overlay_icon);
    if ( notification->icon != NULL )
        eventd_notification_icon_unref(notification->icon);

    g_free(notification->message);
    g_free(notification->title);

    g_free(notification);
}

static GHashTable *
eventd_notification_event_action(EventdEvent *event)
{
    gchar *name;
    EventdNotificationEvent *notification_event;
    EventdNotificationNotification *notification;
    GHashTable *ret = NULL;

    name = g_strconcat(event->client->type, "-", event->type, NULL);
    notification_event = g_hash_table_lookup(events, name);
    g_free(name);
    if ( ( notification_event == NULL ) && ( ( notification_event = g_hash_table_lookup(events, event->client->type) ) == NULL ) )
        return NULL;

    if ( notification_event->disable )
        return NULL;

    notification = eventd_notification_notification_new(event, notification_event);

    switch ( event->client->mode )
    {
    case EVENTD_CLIENT_MODE_PING_PONG:
        ret = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        eventd_notification_notification_insert_data_in_hash_table(notification, ret);
    break;
    default:
        eventd_notification_notify_event_action(notification);
    }

    eventd_notification_notification_free(notification);

    return ret;
}


static void
eventd_notification_config_init()
{
    events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)eventd_notification_event_free);
}

static void
eventd_notification_config_clean()
{
    g_hash_table_unref(events);
}

void
eventd_plugin_get_info(EventdPlugin *plugin)
{
    plugin->id = "notification";

    plugin->start = eventd_notification_start;
    plugin->stop = eventd_notification_stop;

    plugin->config_init = eventd_notification_config_init;
    plugin->config_clean = eventd_notification_config_clean;

    plugin->event_parse = eventd_notification_event_parse;
    plugin->event_action = eventd_notification_event_action;
}

