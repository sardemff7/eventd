/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2018 Quentin "Sardem FF7" Glidic
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

#include "config.h"

#include <errno.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include "libeventd-test.h"

gchar *
connection_test(GDataInputStream *input, GDataOutputStream *output, const gchar *filename, const gchar *message, GError **error)
{
    gchar *tmp;
    gchar *m = NULL;
    gchar *r = NULL;
    gchar *e = NULL;


    if ( ! g_data_output_stream_put_string(output, "SUBSCRIBE test\n", NULL, error) ) goto fail;

    if ( ! g_data_output_stream_put_string(output, ".EVENT 2e6894bb-cf96-462e-a435-766c9b1b4f8a test test\n", NULL, error) ) goto fail;
    tmp = g_strescape(filename, NULL);
    m = g_strdup_printf("DATA file '%s'\n", tmp);
    g_free(tmp);
    if ( ! g_data_output_stream_put_string(output, m, NULL, error) ) goto fail;
    g_free(m);
    m = g_strdup_printf("DATA test '%s'\n", message);
    if ( ! g_data_output_stream_put_string(output, m, NULL, error) ) goto fail;
    m = (g_free(m), NULL);
    if ( ! g_data_output_stream_put_string(output, ".\n", NULL, error) ) goto fail;

    r = g_data_input_stream_read_upto(input, "\n", -1, NULL, NULL, error);
    if ( r == NULL ) goto fail;
    if ( ! g_data_input_stream_read_byte(input, NULL, error) ) goto fail;
    if ( g_strcmp0(r, "EVENT cedb8a77-b7fb-4e32-b3e4-3a772664f1f4 test answer") != 0 )
    {
        e = g_strdup_printf("No or bad answer EVENT: %s", r);
        goto fail;
    }
    r = (g_free(r), NULL);

    gchar *contents;
    if ( ! g_file_get_contents(filename, &contents, NULL, error) ) goto fail;
    if ( g_strcmp0(message, contents) != 0 )
    {
        e = g_strdup_printf("Wrong test file contents: %s", contents);
        g_free(contents);
        goto fail;
    }
    g_free(contents);
    if ( g_unlink(filename) < 0 )
    {
        e = g_strdup_printf("Couldn't remove the file: %s", g_strerror(errno));
        goto fail;
    }



    /* Sending unknown messages to test the proper ignoring behaviour */

    /* Empty line */
    if ( ! g_data_output_stream_put_string(output, "\n", NULL, error) ) goto fail;

    /* Single line */
    if ( ! g_data_output_stream_put_string(output, "TEST\n", NULL, error) ) goto fail;

    /* Dot message */
    if ( ! g_data_output_stream_put_string(output, ".TEST\n", NULL, error) ) goto fail;
    if ( ! g_data_output_stream_put_string(output, ".\n", NULL, error) ) goto fail;

    /* Dot message with data */
    if ( ! g_data_output_stream_put_string(output, ".TEST\n", NULL, error) ) goto fail;
    if ( ! g_data_output_stream_put_string(output, ".DATA data1\n", NULL, error) ) goto fail;
    if ( ! g_data_output_stream_put_string(output, "..some data\n", NULL, error) ) goto fail;
    if ( ! g_data_output_stream_put_string(output, ".\n", NULL, error) ) goto fail;
    if ( ! g_data_output_stream_put_string(output, "DATA data2\n", NULL, error) ) goto fail;
    if ( ! g_data_output_stream_put_string(output, ".\n", NULL, error) ) goto fail;

    /* Two nested dot messages with data */
    if ( ! g_data_output_stream_put_string(output, ".TEST\n", NULL, error) ) goto fail;
    if ( ! g_data_output_stream_put_string(output, ".EVENT 3 test test\n", NULL, error) ) goto fail;
    if ( ! g_data_output_stream_put_string(output, ".DATA data1\n", NULL, error) ) goto fail;
    if ( ! g_data_output_stream_put_string(output, "..some data\n", NULL, error) ) goto fail;
    if ( ! g_data_output_stream_put_string(output, ".\n", NULL, error) ) goto fail;
    if ( ! g_data_output_stream_put_string(output, ".\n", NULL, error) ) goto fail;
    if ( ! g_data_output_stream_put_string(output, ".\n", NULL, error) ) goto fail;


    /* Sending a second event to test that everything is fine */

    if ( ! g_data_output_stream_put_string(output, ".EVENT 8d099ddd-2b3b-4bd6-8ff7-374632032493 test test\n", NULL, error) ) goto fail;
    if ( ! g_data_output_stream_put_string(output, ".\n", NULL, error) ) goto fail;

    /* Sending a third event to test empty newline in DATA */

    if ( ! g_data_output_stream_put_string(output, ".EVENT 2fcdb771-9307-448d-89ab-33cbf8047cc9 test test\n", NULL, error) ) goto fail;
    if ( ! g_data_output_stream_put_string(output, ".DATA test\n", NULL, error) ) goto fail;
    if ( ! g_data_output_stream_put_string(output, "\n", NULL, error) ) goto fail;
    if ( ! g_data_output_stream_put_string(output, ".\n", NULL, error) ) goto fail;
    if ( ! g_data_output_stream_put_string(output, ".\n", NULL, error) ) goto fail;

    g_data_output_stream_put_string(output, "BYE\n", NULL, error);

    return NULL;

fail:
    g_free(r);
    g_free(m);
    return e;
}


int
main(int argc, char *argv[])
{
    eventd_tests_env_setup(argv, "evp-connection");

    EventdTestsEnv *env = eventd_tests_env_new("tcp:localhost:19011", NULL, FALSE);

    int r = 99;
    if ( ! eventd_tests_env_start_eventd(env) )
        goto end;

    r = 0;

    GSocketClient *client = g_socket_client_new();
    GInetAddress *inet_address = g_inet_address_new_loopback(G_SOCKET_FAMILY_IPV4);
    GSocketConnectable *address = G_SOCKET_CONNECTABLE(g_inet_socket_address_new(inet_address, 19011));
    g_object_unref(inet_address);
    GSocketConnection *connection;
    GDataInputStream *input;
    GDataOutputStream *output;

    gchar *filename = g_build_filename(g_getenv("XDG_RUNTIME_DIR"), "evp-connection-file", NULL);
    const gchar *message = "Some message";

    GError *error = NULL;

    connection = g_socket_client_connect(client, address, NULL, &error);
    g_object_unref(client);
    if ( connection == NULL )
        goto error;

    input = g_data_input_stream_new(g_io_stream_get_input_stream(G_IO_STREAM(connection)));
    output = g_data_output_stream_new(g_io_stream_get_output_stream(G_IO_STREAM(connection)));

    gchar *e = connection_test(input, output, filename, message, &error);
    g_object_unref(input);
    g_object_unref(output);
    if ( e != NULL )
    {
        r = 1;
        g_warning("Test failed: %s", e);
    }
    g_io_stream_close(G_IO_STREAM(connection), NULL, &error);

    g_object_unref(connection);
error:
    g_free(filename);
    if ( error != NULL )
    {
        g_warning("Test failed: %s", error->message);
        g_error_free(error);
        r = 99;
    }

    if ( ! eventd_tests_env_stop_eventd(env) )
        r = 99;

end:
    eventd_tests_env_free(env);
    return r;
}
