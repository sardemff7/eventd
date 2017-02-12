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
libeventc_sources = [
        'src/libeventc.c',
]
libeventc_lib = library('eventc', [
        config_h,
        libeventc_sources,
    ],
    c_args: [
        '-DG_LOG_DOMAIN="libeventc"',
    ],
    dependencies: [ ws_module, libeventc_dep ],
    version: '0.0.0',
    include_directories: libeventc_inc,
    install: true
)

libeventc_headers = [
    'include/libeventc.h',
]
install_headers(libeventc_headers,
    subdir: meson.project_name(),
)

libeventc = declare_dependency(link_with: libeventc_lib, include_directories: libeventc_inc, dependencies: libeventc_dep)

pkgconfig.generate(
    filebase: 'libeventc',
    name: 'libeventc',
    version: meson.project_version(),
    description: 'Library to communicate with eventd',
    libraries: libeventc_lib,
    subdirs: 'eventd',
    requires_private: [
        'libeventd',
        'gio-2.0',
        'gobject-2.0',
        'glib-2.0',
    ],
)

if get_option('enable-gobject-introspection')
    libeventd_plugin_gir = gnome.generate_gir(libeventc_lib,
        dependencies: libeventc,
        namespace: 'Eventc',
        nsversion: '0',
        sources: libeventc_sources + libeventc_headers,
        includes: [ libeventd_gir[0], 'Gio-2.0', 'GObject-2.0' ],
        install: true,
    )
endif
