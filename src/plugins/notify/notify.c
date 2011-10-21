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

#include <libnotify/notify.h>

#include <glib.h>

#include <eventd-plugin.h>
#include <plugin-helper.h>


typedef struct {
    gchar *title;
    gchar *message;
} EventdNotifyEvent;

static GHashTable *events = NULL;

static void
eventd_notify_start(gpointer user_data)
{
    notify_init(PACKAGE_NAME);

    eventd_plugin_helper_regex_init();
}

static void
eventd_notify_stop()
{
    eventd_plugin_helper_regex_clean();

    notify_uninit();
}

static gboolean
notification_closed_cb(NotifyNotification *notification)
{
    return FALSE;
}

static void
eventd_notify_event_clean(EventdNotifyEvent *event)
{
    g_free(event->message);
    g_free(event->title);
}

static void
eventd_notify_event_update(EventdNotifyEvent *event, const char *title, const char *message)
{
    eventd_notify_event_clean(event);
    event->title = g_strdup(title ? title : "$client-name - $event-name");
    event->message = g_strdup(message ? message : "$event-data[text]");
}

static EventdNotifyEvent *
eventd_notify_event_new(const char *title, const char *message)
{
    EventdNotifyEvent *event = NULL;

    event = g_new0(EventdNotifyEvent, 1);

    eventd_notify_event_update(event, title, message);

    return event;
}

static void
eventd_notify_event_free(EventdNotifyEvent *event)
{
    eventd_notify_event_clean(event);
    g_free(event);
}

static void
eventd_notify_event_parse(const gchar *client_type, const gchar *event_type, GKeyFile *config_file)
{
    gchar *name = NULL;
    gchar *title = NULL;
    gchar *message = NULL;
    EventdNotifyEvent *event;

    if ( ! g_key_file_has_group(config_file, "notify") )
        return;

    if ( eventd_plugin_helper_config_key_file_get_string(config_file, "notify", "title", &title) < 0 )
        goto skip;
    if ( eventd_plugin_helper_config_key_file_get_string(config_file, "notify", "message", &message) < 0 )
        goto skip;

    if ( event_type != NULL )
        name = g_strdup_printf("%s-%s", client_type, event_type);
    else
        name = g_strdup(client_type);

    event = g_hash_table_lookup(events, name);
    if ( event != NULL )
        eventd_notify_event_update(event, title, message);
    else
        g_hash_table_insert(events, name, eventd_notify_event_new(title, message));

skip:
    g_free(message);
    g_free(title);
}

static void
eventd_notify_event_action(const gchar *client_type, const gchar *client_name, const gchar *event_type, const gchar *event_name, const GHashTable *event_data)
{
    gchar *name;
    GError *error = NULL;
    EventdNotifyEvent *event;
    gchar *title = NULL;
    gchar *message = NULL;
    gchar *tmp = NULL;
    NotifyNotification *notification = NULL;

    name = g_strdup_printf("%s-%s", client_type, event_type);

    event = g_hash_table_lookup(events, name);
    if ( ( event == NULL ) && ( ( event = g_hash_table_lookup(events, event_type) ) == NULL ) )
        goto fail;

    tmp = eventd_plugin_helper_regex_replace_client_name(event->title, client_name);
    title = eventd_plugin_helper_regex_replace_event_name(tmp, event_name);
    g_free(tmp);

    message = eventd_plugin_helper_regex_replace_event_data(event->message, event_data, NULL);

    notification = notify_notification_new(title, message, NULL
    #if ! NOTIFY_CHECK_VERSION(0,7,0)
    , NULL
    #endif
    );
    notify_notification_set_urgency(notification, NOTIFY_URGENCY_NORMAL);
    notify_notification_set_timeout(notification, 1);
    if ( ! notify_notification_show(notification, &error) )
        g_warning("Can't show the notification: %s", error->message);
    g_clear_error(&error);
    g_signal_connect(notification, "closed", G_CALLBACK(notification_closed_cb), NULL);
    g_object_unref(G_OBJECT(notification));
    g_free(message);
    g_free(title);

fail:
    g_free(name);
}


static void
eventd_notify_config_init()
{
    events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)eventd_notify_event_free);
}

static void
eventd_notify_config_clean()
{
    g_hash_table_unref(events);
}

void
eventd_plugin_get_info(EventdPlugin *plugin)
{
    plugin->start = eventd_notify_start;
    plugin->stop = eventd_notify_stop;

    plugin->config_init = eventd_notify_config_init;
    plugin->config_clean = eventd_notify_config_clean;

    plugin->event_parse = eventd_notify_event_parse;
    plugin->event_action = eventd_notify_event_action;
}

