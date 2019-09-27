# Internal helper library

libeventd_helpers_inc = include_directories('include')
libeventd_helpers_dep = [
    libeventd,
    gio,
    glib,
]
libeventd_helpers_lib = library('eventd-helpers', config_h, files(
        'src/reconnect.c',
        'src/config.c',
        'include/libeventd-helpers-dirs.h',
    ),
    c_args: [
        '-DG_LOG_DOMAIN="libeventd-helpers"',
    ],
    dependencies: [ libnkutils, libeventd_helpers_dep ],
    include_directories: libeventd_helpers_inc,
    install: true,
)

install_headers(
    'include/libeventd-helpers-config.h',
    'include/libeventd-helpers-reconnect.h',
    subdir: meson.project_name(),
)

libeventd_helpers = declare_dependency(link_with: libeventd_helpers_lib, include_directories: libeventd_helpers_inc, dependencies: libeventd_helpers_dep)
