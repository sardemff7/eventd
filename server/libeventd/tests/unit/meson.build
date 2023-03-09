libeventd_event_private = libeventd_lib.extract_objects('src/event-private.c')
libeventd_event_test = executable('libeventd-event.test', files(
        'common.h',
        'event-getters.c',
        'event-getters.h',
        'event-setters.c',
        'event-setters.h',
        'libeventd-event.c',
    ),
    objects: libeventd_event_private,
    dependencies: [ libnkutils_uuid, libeventd ],
)
test('libeventd-event unit tests', libeventd_event_test,
    suite: [ 'unit', 'libeventd' ],
    args: [ '--tap' ],
    protocol: 'tap',
)

libeventd_protocol_test = executable('libeventd-protocol.test', files(
        'common.h',
        'protocol-parser.c',
        'protocol-parser.h',
        'protocol-generator.c',
        'protocol-generator.h',
        'libeventd-protocol.c',
    ),
    objects: libeventd_event_private,
    dependencies: [ libnkutils_uuid, libeventd ],
)
test('libeventd-protocol unit tests', libeventd_protocol_test,
    suite: [ 'unit', 'libeventd' ],
    args: [ '--tap' ],
    protocol: 'tap',
)
