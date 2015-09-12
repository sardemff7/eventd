/*
 * libeventd-evp - Library to send EVENT protocol messages
 *
 * Copyright © 2011-2012 Quentin "Sardem FF7" Glidic
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

#ifndef __LIBEVENTD_EVP_H__
#define __LIBEVENTD_EVP_H__

#include <glib.h>
#include <gio/gio.h>

#include <libeventd-event.h>


/*
 * Helpers
 */

GSocketConnectable *libeventd_evp_get_address(const gchar *host_and_port, GError **error);


/*
 * Connection handling
 */

typedef struct _LibeventdEvpContext LibeventdEvpContext;
typedef void LibeventdEvpClientIface;

typedef void (*LibeventdEvpErrorCallback)(gpointer client, LibeventdEvpContext *context, GError *error);
typedef void (*LibeventdEvpCallback)(gpointer client, LibeventdEvpContext *context);
typedef void (*LibeventdEvpEventCallback)(gpointer client, LibeventdEvpContext *context, const gchar *id, EventdEvent *event);
typedef void (*LibeventdEvpEndCallback)(gpointer client, LibeventdEvpContext *context, const gchar *id);
typedef void (*LibeventdEvpAnsweredCallback)(gpointer client, LibeventdEvpContext *context, const gchar *id, const gchar *answer, GHashTable *data_hash);
typedef void (*LibeventdEvpEndedCallback)(gpointer client, LibeventdEvpContext *context, const gchar *id, EventdEventEndReason reason);

typedef struct {
    LibeventdEvpErrorCallback error;

    LibeventdEvpEventCallback event;
    LibeventdEvpEndCallback end;

    LibeventdEvpAnsweredCallback answered;
    LibeventdEvpEndedCallback ended;

    LibeventdEvpCallback bye;
} LibeventdEvpClientInterface;

LibeventdEvpContext *libeventd_evp_context_new(gpointer client, LibeventdEvpClientInterface *interface);
LibeventdEvpContext *libeventd_evp_context_new_for_connection(gpointer client, LibeventdEvpClientInterface *interface, GSocketConnection *connection);
void libeventd_evp_context_free(LibeventdEvpContext *context);

gboolean libeventd_evp_context_is_connected(LibeventdEvpContext *contex, GError **errort);

void libeventd_evp_context_set_connection(LibeventdEvpContext *context, GSocketConnection *connection);
void libeventd_evp_context_close(LibeventdEvpContext *context);

gboolean libeventd_evp_context_passive(LibeventdEvpContext *context, GError **error);
void libeventd_evp_context_receive_loop(LibeventdEvpContext *context, gint priority);

gboolean libeventd_evp_context_send_event(LibeventdEvpContext *context, const gchar *id, EventdEvent *event, GError **error);
gboolean libeventd_evp_context_send_end(LibeventdEvpContext *context, const gchar *id, GError **error);
gboolean libeventd_evp_context_send_answered(LibeventdEvpContext *context, const gchar *id, const gchar *answer, GHashTable *data, GError **error);
gboolean libeventd_evp_context_send_ended(LibeventdEvpContext *context, const gchar *id, EventdEventEndReason reason, GError **error);
void libeventd_evp_context_send_bye(LibeventdEvpContext *context);

#endif /* __LIBEVENTD_EVP_H__ */
