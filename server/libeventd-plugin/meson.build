# Plugin and core interfaces manipulation library

libeventd_plugin_inc = include_directories('include')
libeventd_plugin_dep = [
    libeventd,
    gobject,
    glib,
]
libeventd_plugin_sources = files(
        'src/core.c',
        'src/plugin.c',
)
libeventd_plugin_lib = library('eventd-plugin', [ config_h ] + files(
        'include/eventd-plugin-private.h',
    ) + libeventd_plugin_sources,
    c_args: [
        '-DG_LOG_DOMAIN="libeventd-plugin"',
    ],
    dependencies: libeventd_plugin_dep,
    version: '0.0.0',
    include_directories: libeventd_plugin_inc,
    install: true,
)

libeventd_plugin_headers = files(
    'include/eventd-plugin.h',
)
install_headers(libeventd_plugin_headers,
    subdir: meson.project_name(),
)

libeventd_plugin = declare_dependency(link_with: libeventd_plugin_lib, include_directories: libeventd_plugin_inc, dependencies: libeventd_plugin_dep)

pkgconfig.generate(
    filebase: 'libeventd-plugin',
    name: 'libeventd-plugin',
    version: meson.project_version(),
    description: 'Library to implement an eventd plugin',
    libraries: libeventd_plugin_lib,
    subdirs: 'eventd',
    requires_private: [
        'libeventd',
        'gio-2.0',
        'gobject-2.0',
        'glib-2.0',
    ],
    variables: [
        'pluginsdir=@0@'.format(join_paths('${prefix}', plugins_install_dir)),
    ],
)

if get_option('gobject-introspection')
    libeventd_plugin_gir = gnome.generate_gir(libeventd_plugin_lib,
        dependencies: libeventd_plugin,
        namespace: 'EventdPlugin',
        nsversion: '0',
        sources: libeventd_plugin_sources + libeventd_plugin_headers,
        includes: [ libeventd_gir[0], 'Gio-2.0', 'GObject-2.0' ],
        extra_args: [
            '--c-include=eventd-plugin.h',
        ],
        install: true,
    )
endif
