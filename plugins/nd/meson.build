# notification plugin

wayland_min_version='1.9.91'
ww_min_version='1'

headers = [
    'sys/mman.h'
]
foreach h : headers
    if not c_compiler.has_header(h)
        error('Header @0@ was not found, but is required'.format(h))
    endif
endforeach

gdk_pixbuf = dependency('gdk-pixbuf-2.0')
cairo = dependency('cairo')
pango = [
    dependency('pango'),
    dependency('pangocairo'),
]
libm = c_compiler.find_library('m', required: false)

if not libnkutils_bindings.found()
    error('libxkbcommon is required for the nd plugin')
endif

librt = c_compiler.find_library('rt', required: false)
wayland_wall = dependency('wayland-wall', version: '>= @0@'.format(ww_min_version))
wayland = [
    subproject('libgwater').get_variable('libgwater_wayland'),
    dependency('wayland-client', version: '>= @0@'.format(wayland_min_version)),
    dependency('wayland-cursor'),
    wayland_wall,
]
wayland_scanner = find_program('wayland-scanner')
ww_protocol_dir = wayland_wall.get_pkgconfig_variable('pkgdatadir')

wayland_scanner_client = generator(wayland_scanner, output: '@BASENAME@-client-protocol.h', arguments: ['client-header', '@INPUT@', '@OUTPUT@'])
wayland_scanner_server = generator(wayland_scanner, output: '@BASENAME@-server-protocol.h', arguments: ['server-header', '@INPUT@', '@OUTPUT@'])
wayland_scanner_code_command = wayland[2].version().version_compare('>= 1.14.91') ? 'private-code' : 'code'
wayland_scanner_code = generator(wayland_scanner, output: '@BASENAME@-protocol.c', arguments: [wayland_scanner_code_command, '@INPUT@', '@OUTPUT@'])

shared_library('nd', config_h, files(
        'src/types.h',
        'src/nd.h',
        'src/nd.c',
        'src/notification.h',
        'src/notification.c',
        'src/draw.c',
        'src/draw.h',
        'src/blur.c',
        'src/blur.h',
        'src/style.c',
        'src/style.h',
        'src/pixbuf.h',
        'src/pixbuf.c',
        'src/wayland.h',
        'src/wayland.c',
    ) + [
            wayland_scanner_client.process(join_paths(ww_protocol_dir, 'unstable/notification-area/notification-area-unstable-v1.xml')),
            wayland_scanner_code.process(join_paths(ww_protocol_dir, 'unstable/notification-area/notification-area-unstable-v1.xml')),
        ],
    c_args: [
        '-DG_LOG_DOMAIN="eventd-nd"',
    ],
    dependencies: [
        wayland,
        librt,
        gdk_pixbuf,
        pango,
        cairo,
        libm,
        libeventd_helpers,
        libeventd_plugin,
        libeventd,
        libnkutils_bindings,
        gmodule,
        gobject,
        glib,
    ],
    name_prefix: '',
    install: true,
    install_dir: plugins_install_dir,
)

man_pages += [ [ files('man/eventdctl-nd.xml'), 'eventdctl-nd.1' ] ]
man_pages += [ [ files('man/eventd-nd.conf.xml'), 'eventd-nd.conf.5' ] ]
docbook_conditions += 'enable_notification_daemon'
