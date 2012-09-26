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

#ifndef __LIBEVENTD_EVP_CONTEXT_H__
#define __LIBEVENTD_EVP_CONTEXT_H__

#include <libeventd-evp.h>

typedef void (*LibeventdEvpWaiterCallback)(LibeventdEvpContext *context, const gchar *line);

struct _LibeventdEvpContext {
    gpointer client;
    LibeventdEvpClientInterface *interface;

    GMutex *mutex;
    GCancellable *cancellable;
    GSocketConnection *connection;
    GDataInputStream  *in;
    GDataOutputStream *out;
    GError *error;
    gint priority;
    gboolean server;

    struct {
        LibeventdEvpWaiterCallback callback;
        GSimpleAsyncResult *result;
        EventdEvent *event;
    } waiter;

#if GLIB_CHECK_VERSION(2,32,0)
    GMutex mutex_;
#endif /* ! GLIB_CHECK_VERSION(2,32,0) */
};

gboolean libeventd_evp_context_send_message(LibeventdEvpContext *context, const gchar *message, GError** error);

#endif /* __LIBEVENTD_EVP_CONTEXT_H__ */
