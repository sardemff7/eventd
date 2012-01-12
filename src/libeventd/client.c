/*
 * libeventd - Internal helper
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

#include <libeventd-client.h>

struct _EventdClient {
    gchar *type;
    gchar *name;

    guint64 ref_count;
};

EventdClient *
libeventd_client_new()
{
    EventdClient *client;

    client = g_new0(EventdClient, 1);
    client->ref_count = 1;

    return client;
}

static void
_libeventd_client_finalize(EventdClient *client)
{
    g_free(client->name);
    g_free(client->type);
    g_free(client);
}

EventdClient *
libeventd_client_ref(EventdClient *client)
{
    if ( client != NULL )
        ++client->ref_count;

    return client;
}

void
libeventd_client_unref(EventdClient *client)
{
    if ( client == NULL )
        return;

    if ( --client->ref_count < 1 )
        _libeventd_client_finalize(client);
}

void
libeventd_client_update(EventdClient *client, const gchar *type, const gchar *name)
{
    client->type = (g_free(client->type), NULL);
    if ( type != NULL )
        client->type = g_strdup(type);

    client->name = (g_free(client->name), NULL);
    if ( name != NULL )
        client->name = g_strdup(name);
}


const gchar *
libeventd_client_get_type(EventdClient *client)
{
    return client->type;
}

const gchar *
libeventd_client_get_name(EventdClient *client)
{
    return client->name;
}
