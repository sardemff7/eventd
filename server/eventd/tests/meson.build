evp_connection_test = executable('evp-connection.test', [
        config_h,
        'integration/evp-connection.c',
    ],
    dependencies: [ libeventd_test, libeventc, libeventd, gio, gobject, glib ]
)
test('eventd EvP integration test', evp_connection_test, timeout: 2)

relay_connection_test = executable('relay-connection.test', [
        config_h,
        'integration/relay-connection.c',
    ],
    dependencies: [ libeventd_test, libeventc, libeventd, gio, gobject, glib ]
)
test('eventd relay integration test', relay_connection_test, timeout: 7)
