evp_connection_test = executable('evp-connection.test', [ config_h ] + files(
        'integration/evp-connection.c',
    ),
    dependencies: [ libeventd_test, libeventc, libeventd, gio, gobject, glib ]
)
test('eventd EvP integration test', evp_connection_test,
    suite: [ 'integration', 'eventd' ],
    timeout: 6
)

relay_connection_test = executable('relay-connection.test', [ config_h ] + files(
        'integration/relay-connection.c',
    ),
    dependencies: [ libeventd_test, libeventc, libeventd, gio, gobject, glib ]
)
test('eventd relay integration test', relay_connection_test,
    suite: [ 'integration', 'eventd', 'relay' ],
    timeout: 12
)

ws_connection_test = executable('ws-connection.test', [ config_h ] + files(
        'integration/ws-connection.c',
    ),
    dependencies: [ libeventd_test, libeventc, libeventd, gio, gobject, glib ]
)
test('eventd ws integration test', ws_connection_test,
    suite: [ 'integration', 'eventd', 'ws' ],
    timeout: 9,
    should_fail: not libsoup.found()
)
