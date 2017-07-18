# notification plugin

gdk_pixbuf = dependency('gdk-pixbuf-2.0')
cairo = dependency('cairo')
pango = [
    dependency('pango'),
    dependency('pangocairo'),
]
cc = meson.get_compiler('c')
libm = cc.find_library('m', required: false)

nd_backends_install_dir = join_paths(modules_install_dir, 'nd')

nd_c_args = [
    '-DG_LOG_DOMAIN="eventd-nd"',
]
nd_backend_deps = [
    libeventd_helpers,
    libeventd_plugin,
    libeventd,
    cairo,
    glib,
]
nd_has_backend = false

if is_unix and get_option('enable-nd-wayland')
    nd_has_backend = true
    docbook_conditions += 'enable_nd_wayland'
    nd_c_args += '-DENABLE_ND_WAYLAND'

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
    wayland_wall = dependency('wayland-wall', version: '>= @0@'.format(ww_min_version))
    wayland = [
        subproject('libgwater/wayland').get_variable('libgwater_wayland'),
        dependency('wayland-client', version: '>= @0@'.format(wayland_min_version)),
        dependency('wayland-cursor'),
        wayland_wall,
    ]
    wayland_scanner = find_program('wayland-scanner')
    ww_protocol_dir = wayland_wall.get_pkgconfig_variable('pkgdatadir')

    wayland_scanner_client = generator(wayland_scanner, output: '@BASENAME@-client-protocol.h', arguments: ['client-header', '@INPUT@', '@OUTPUT@'])
    wayland_scanner_server = generator(wayland_scanner, output: '@BASENAME@-server-protocol.h', arguments: ['server-header', '@INPUT@', '@OUTPUT@'])
    wayland_scanner_code = generator(wayland_scanner, output: '@BASENAME@-protocol.c', arguments: ['code', '@INPUT@', '@OUTPUT@'])

    shared_library('wayland', [
            'src/backend.h',
            'src/backend-wayland.c',
            wayland_scanner_client.process(join_paths(ww_protocol_dir, 'unstable/notification-area/notification-area-unstable-v1.xml')),
            wayland_scanner_code.process(join_paths(ww_protocol_dir, 'unstable/notification-area/notification-area-unstable-v1.xml')),
        ],
        c_args: [
            '-DG_LOG_DOMAIN="eventd-nd-wayland"',
        ],
        dependencies: [ wayland, nd_backend_deps ],
        name_prefix: '',
        install: true,
        install_dir: nd_backends_install_dir,
    )
endif

if is_unix and get_option('enable-nd-xcb')
    nd_has_backend = true
    docbook_conditions += 'enable_nd_xcb'
    nd_c_args += '-DENABLE_ND_XCB'

    xcb = [
        subproject('libgwater/xcb').get_variable('libgwater_xcb'),
        dependency('cairo-xcb'),
        dependency('xcb'),
        dependency('xcb-aux'),
        dependency('xcb-randr'),
        dependency('xcb-ewmh'),
        dependency('xcb-xfixes'),
        dependency('xcb-shape'),
    ]

    shared_library('xcb', [
            'src/backend.h',
            'src/backend-xcb.c',
        ],
        c_args: [
            '-DG_LOG_DOMAIN="eventd-nd-xcb"',
        ],
        dependencies: [ xcb, nd_backend_deps ],
        name_prefix: '',
        install: true,
        install_dir: nd_backends_install_dir,
    )
endif

if is_unix and get_option('enable-nd-fbdev')
    nd_has_backend = true
    docbook_conditions += 'enable_nd_fbdev'
    nd_c_args += '-DENABLE_ND_FBDEV'

    shared_library('fbdev', [
            'src/backend.h',
            'src/backend-fbdev.c',
        ],
        c_args: [
            '-DG_LOG_DOMAIN="eventd-nd-fbdev"',
        ],
        dependencies: nd_backend_deps,
        name_prefix: '',
        install: true,
        install_dir: nd_backends_install_dir,
    )
endif

if is_windows
    nd_has_backend = true
    docbook_conditions += 'enable_nd_win'
    nd_c_args += '-DENABLE_ND_WIN'

    win = [
        subproject('libgwater/win').get_variable('libgwater_win'),
        dependency('cairo-win32'),
    ]

    shared_library('win', [
            'src/backend.h',
            'src/backend-win.c',
        ],
        c_args: [
            '-DG_LOG_DOMAIN="eventd-nd-win"',
        ],
        dependencies: [ win, nd_backend_deps ],
        name_prefix: '',
        install: true,
        install_dir: nd_backends_install_dir,
    )
endif

if not nd_has_backend
    error('notification-daemon plugin required but no backend selected')
endif

shared_library('nd', [
        config_h,
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
        'src/backend.h',
        'src/backends.h',
        'src/backends.c',
    ],
    c_args: nd_c_args,
    dependencies: [
        gdk_pixbuf,
        pango,
        cairo,
        libm,
        libeventd_helpers,
        libeventd_plugin,
        libeventd,
        libnkutils,
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
