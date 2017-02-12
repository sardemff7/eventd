# Client library - light (local-only non-GIO) version

libeventc_light_dep = []
if is_unix
    headers = [
        'sys/un.h',
        'netinet/in.h',
    ]
else
    headers = [
        'winsock2.h',
        'windows.h',
    ]
    libeventc_light_dep += c_compiler.find_library('ws2_32')
endif

foreach h : headers
    if not c_compiler.has_header(h)
        error('Header @0@ was not found, but is required'.format(h))
    endif
endforeach

libeventc_light_inc = include_directories('include')
libeventc_light_dep += [
    libeventd,
    glib,
]
libeventc_light_lib = library('eventc-light', [
        config_h,
        'src/libeventc-light.c',
    ],
    c_args: [
        '-DG_LOG_DOMAIN="libeventc-light"',
    ],
    dependencies: libeventc_light_dep,
    version: '0.0.0',
    include_directories: libeventc_light_inc,
    install: true
)

install_headers(
    'include/libeventc-light.h',
    subdir: meson.project_name(),
)

libeventc_light = declare_dependency(link_with: libeventc_light_lib, include_directories: libeventc_light_inc, dependencies: libeventc_light_dep)

pkgconfig.generate(
    filebase: 'libeventc-light',
    name: 'libeventc/light',
    version: meson.project_version(),
    description: 'Library to communicate with eventd, light (local-only non-GIO) version',
    libraries: libeventc_light_lib,
    subdirs: 'eventd',
    requires_private: [
        'libeventd',
        'glib-2.0'
    ],
)
