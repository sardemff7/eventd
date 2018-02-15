/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2018 Quentin "Sardem FF7" Glidic
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with eventd. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __EVENTD_GIO_COMPAT_H__
#define __EVENTD_GIO_COMPAT_H__

#if ! GLIB_CHECK_VERSION(2, 44, 0)
#define g_network_address_new_loopback(p) g_network_address_new("localhost", p)
#endif

#if ! GLIB_CHECK_VERSION(2, 48, 0)
static inline gchar *
g_socket_connectable_to_string(GSocketConnectable *c)
{
    if ( G_IS_UNIX_SOCKET_ADDRESS(c) )
        return g_strdup(g_unix_socket_address_get_path(G_UNIX_SOCKET_ADDRESS(c)));
    if ( G_IS_NETWORK_SERVICE(c) )
        return g_strdup(g_network_service_get_domain(G_NETWORK_SERVICE(c)));
    if ( G_IS_NETWORK_ADDRESS(c) )
        return g_strdup_printf("%s:%u", g_network_address_get_hostname(G_NETWORK_ADDRESS(c)),  g_network_address_get_port(G_NETWORK_ADDRESS(c)));
    if ( G_IS_PROXY_ADDRESS(c) )
        return g_strdup_printf("%s:%u", g_proxy_address_get_destination_hostname(G_PROXY_ADDRESS(c)),  g_proxy_address_get_destination_port(G_PROXY_ADDRESS(c)));
    if ( G_IS_INET_SOCKET_ADDRESS(c) )
    {
        GInetAddress *inet_address;
        gchar *str, *tmp;
        inet_address = g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(c));
        tmp = g_inet_address_to_string(inet_address);
        str = g_strdup_printf("%s:%u", tmp,  g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(c)));
        g_free(tmp);
        return str;
    }
    return g_strdup("");
}
#endif

#endif /* __EVENTD_GIO_COMPAT_H__ */
