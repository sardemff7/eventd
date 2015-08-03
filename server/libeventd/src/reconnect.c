/*
 * libeventd - Internal helper
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <glib.h>

#include <libeventd-reconnect.h>

struct _LibeventdReconnectHandler {
    gint64 timeout;
    gint64 max_tries;
    LibeventdReconnectTryCallback callback;
    gpointer user_data;
    guint64 try;
    guint timeout_tag;
    GDestroyNotify notify;
};

EVENTD_EXPORT
LibeventdReconnectHandler *
libeventd_reconnect_new(gint64 timeout, gint64 max_tries, LibeventdReconnectTryCallback callback, gpointer user_data, GDestroyNotify notify)
{
    g_return_val_if_fail(callback != NULL, NULL);

    LibeventdReconnectHandler *self;

    self = g_slice_new0(LibeventdReconnectHandler);

    self->timeout = timeout;
    self->max_tries = max_tries;
    self->callback = callback;
    self->user_data = user_data;
    self->notify = notify;

    return self;
}

EVENTD_EXPORT
void
libeventd_reconnect_free(LibeventdReconnectHandler *self)
{
    /* dispose of the reconnection data */
    if (self->notify != NULL)
    {
      self->notify(self->user_data);

      self->user_data = NULL;
      self->notify = NULL;
    }

    if ( self->timeout_tag > 0 )
        g_source_remove(self->timeout_tag);
    g_slice_free(LibeventdReconnectHandler, self);
}

static gboolean
_libeventd_reconnect_timeout(gpointer user_data)
{
    LibeventdReconnectHandler *self = user_data;

    self->callback(self, self->user_data);

    self->timeout_tag = 0;
    return FALSE;
}

EVENTD_EXPORT
gboolean
libeventd_reconnect_try(LibeventdReconnectHandler *self)
{
    if ( ( self->max_tries > 0 ) && ( self->try >= (guint64) self->max_tries ) )
        return FALSE;

    self->timeout_tag = g_timeout_add_seconds(self->timeout << self->try, _libeventd_reconnect_timeout, self);
    ++self->try;
    return TRUE;
}

EVENTD_EXPORT
void
libeventd_reconnect_reset(LibeventdReconnectHandler *self)
{
    self->try = 0;
    if ( self->timeout_tag > 0 )
        g_source_remove(self->timeout_tag);
    self->timeout_tag = 0;
}
