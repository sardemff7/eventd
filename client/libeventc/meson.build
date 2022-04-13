# Client library

libeventc_inc = include_directories('include')
libeventc_dep = [
    libeventd,
    gio_platform,
    gio,
    gmodule,
    gobject,
    glib,
]
libeventc_sources = files(
        'src/libeventc.c',
)
libeventc_lib = library('eventc',
    config_h, libeventc_sources,
    c_args: [
        '-DG_LOG_DOMAIN="libeventc"',
    ],
    dependencies: [ ws_module, libnkutils, libeventc_dep ],
    version: '0.0.0',
    include_directories: libeventc_inc,
    install: true
)

libeventc_headers = files(
    'include/libeventc.h',
)
install_headers(libeventc_headers,
    subdir: meson.project_name(),
)

libeventc = declare_dependency(link_with: libeventc_lib, include_directories: libeventc_inc, dependencies: libeventc_dep)

pkgconfig.generate(libeventc_lib,
    filebase: 'libeventc',
    name: 'libeventc',
    version: meson.project_version(),
    description: 'Library to communicate with eventd',
    subdirs: 'eventd',
)

if get_option('gobject-introspection')
    libeventc_gir = gnome.generate_gir(libeventc_lib,
        dependencies: libeventc,
        namespace: 'Eventc',
        nsversion: '0',
        sources: libeventc_sources + libeventc_headers,
        includes: [ libeventd_gir[0], 'GObject-2.0' ],
        extra_args: [
            '--c-include=libeventc.h',
        ],
        install: true,
    )
    if get_option('vapi')
        gnome.generate_vapi('libeventc',
            sources: [ libeventc_gir[0] ],
            install: true,
        )
    endif
endif
