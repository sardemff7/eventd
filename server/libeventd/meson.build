# Main eventd library

libeventd_inc = include_directories('include')
libeventd_dep = [
    gobject,
    glib,
]
libeventd_sources = files(
    'src/event-private.c',
    'src/event.c',
    'src/protocol-evp.c',
    'src/protocol-evp-parser.c',
    'src/protocol-evp-generator.c',
)
libeventd_lib = library('eventd', config_h, files(
        'include/libeventd-event-private.h',
        'include/libeventd-event.h',
        'include/libeventd-protocol.h',
        'src/event-private.h',
        'src/protocol-evp-private.h',
    ), libeventd_sources,
    c_args: [
        '-DG_LOG_DOMAIN="libeventd"',
    ],
    dependencies: [ libnkutils_uuid, libeventd_dep ],
    version: '0.0.0',
    include_directories: libeventd_inc,
    install: true,
)

libeventd_headers = files(
    'include/libeventd-event.h',
    'include/libeventd-protocol.h',
)
install_headers(libeventd_headers,
    subdir: meson.project_name(),
)

libeventd = declare_dependency(link_with: libeventd_lib, include_directories: libeventd_inc, dependencies: libeventd_dep)

pkgconfig.generate(libeventd_lib,
    filebase: 'libeventd',
    name: 'libeventd',
    version: meson.project_version(),
    description: 'Main eventd library',
    subdirs: 'eventd',
)

libeventd_event_private = libeventd_lib.extract_objects('src/event-private.c')
libeventd_event_test = executable('libeventd-event.test', files(
        'tests/unit/common.h',
        'tests/unit/event-getters.c',
        'tests/unit/event-getters.h',
        'tests/unit/event-setters.c',
        'tests/unit/event-setters.h',
        'tests/unit/libeventd-event.c',
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
        'tests/unit/common.h',
        'tests/unit/protocol-parser.c',
        'tests/unit/protocol-parser.h',
        'tests/unit/protocol-generator.c',
        'tests/unit/protocol-generator.h',
        'tests/unit/libeventd-protocol.c',
    ),
    objects: libeventd_event_private,
    dependencies: [ libnkutils_uuid, libeventd ],
)
test('libeventd-protocol unit tests', libeventd_protocol_test,
    suite: [ 'unit', 'libeventd' ],
    args: [ '--tap' ],
    protocol: 'tap',
)

if get_option('gobject-introspection')
    libeventd_gir = gnome.generate_gir(libeventd_lib,
        dependencies: libeventd,
        namespace: 'Eventd',
        nsversion: '0',
        sources: libeventd_sources + libeventd_headers,
        includes: [ 'GObject-2.0' ],
        extra_args: [
            '--c-include=libeventd-event.h',
            '--c-include=libeventd-protocol.h',
        ],
        install: true,
    )
endif
