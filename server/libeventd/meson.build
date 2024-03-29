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

subdir('tests/unit')

if get_option('gobject-introspection')
    libeventd_gir = gnome.generate_gir(libeventd_lib,
        dependencies: libeventd,
        namespace: 'Eventd',
        nsversion: '0',
        sources: libeventd_sources + libeventd_headers,
        includes: [ 'GObject-2.0', 'GLib-2.0' ],
        extra_args: [
            '--c-include=libeventd-event.h',
            '--c-include=libeventd-protocol.h',
        ],
        install: true,
    )
    if get_option('vapi')
        gnome.generate_vapi('libeventd',
            sources: [ libeventd_gir[0] ],
            packages: [ 'gobject-2.0', 'glib-2.0' ],
            install: true,
        )
    endif
endif
