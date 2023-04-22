eventd_private = eventd.extract_objects(
    'src/config.c',
    'src/events.c',
)
eventd_test = executable('eventd.test', files(
        'stubs.c',
        'events.c',
        'eventd.c',
    ),
    objects: eventd_private,
    dependencies: eventd_test_dep,
)
test('eventd unit tests', eventd_test,
    suite: [ 'unit', 'eventd' ],
    args: [ '--tap' ],
    protocol: 'tap',
)
