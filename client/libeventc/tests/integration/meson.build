libeventc_connection_test = executable('libeventc-connection.test', config_h, files(
        'connection.c',
    ),
    dependencies: [ libeventd_test, libeventc, glib ]
)
test('libeventc integration test', libeventc_connection_test,
    suite: [ 'integration', 'libeventc' ],
    timeout: 9
)
