modules_build_dir = meson.current_build_dir()

if get_option('enable-websocket')
    libsoup = dependency('libsoup-2.4', version: '>=2.50')
    ws = shared_module('ws', [
            config_h,
            'include/eventd-ws-module.h',
            'src/ws.c',
        ],
        c_args: [
            '-DG_LOG_DOMAIN="eventd-ws"',
        ],
        include_directories: include_directories('include'),
        dependencies: [
            libsoup,
            libeventc,
            libeventd,
            gio,
            gobject,
            glib,
        ],
        name_prefix: '',
        install: true,
        install_dir: modules_install_dir,
    )
endif

if get_option('enable-dns-sd')
    avahi = [ dependency('avahi-client'), dependency('avahi-glib') ]
    dns_sd = shared_module('dns-sd', [
            config_h,
            'include/eventd-sd-module.h',
            'src/dns-sd.c',
        ],
        c_args: [
            '-DG_LOG_DOMAIN="eventd-dns-sd"',
        ],
        include_directories: include_directories('include'),
        dependencies: [
            avahi,
            libeventd,
            gio,
            gobject,
            glib,
        ],
        name_prefix: '',
        install: true,
        install_dir: modules_install_dir,
    )
endif

if get_option('enable-ssdp')
    gssdp = dependency('gssdp-1.0')
    ssdp = shared_module('ssdp', [
            config_h,
            'include/eventd-sd-module.h',
            'src/ssdp.c',
        ],
        c_args: [
            '-DG_LOG_DOMAIN="eventd-ssdp"',
        ],
        include_directories: include_directories('include'),
        dependencies: [
            gssdp,
            libeventd,
            libnkutils,
            gio,
            gobject,
            glib,
        ],
        name_prefix: '',
        install: true,
        install_dir: modules_install_dir,
    )
endif
