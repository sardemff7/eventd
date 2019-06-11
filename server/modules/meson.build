modules_build_dir = meson.current_build_dir()

libsoup = dependency('libsoup-2.4', version: '>=2.50', required: get_option('websocket'))
if libsoup.found()
    docbook_conditions += 'enable_websocket'
    ws = shared_library('ws', [ config_h ] + files(
            'include/eventd-ws-module.h',
            'src/ws.c',
        ),
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

avahi = dependency('avahi-client', required: get_option('dns-sd'))
avahi_glib = dependency('avahi-glib', required: get_option('dns-sd'))
if avahi.found() and avahi_glib.found()
    docbook_conditions += 'enable_dns_sd'
    dns_sd = shared_library('dns-sd', [ config_h ] + files(
            'include/eventd-sd-module.h',
            'src/dns-sd.c',
        ),
        c_args: [
            '-DG_LOG_DOMAIN="eventd-dns-sd"',
        ],
        include_directories: include_directories('include'),
        dependencies: [
            avahi_glib,
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

gssdp = dependency('gssdp-1.2', required: get_option('ssdp'))
if gssdp.found()
    docbook_conditions += 'enable_ssdp'
    ssdp = shared_library('ssdp', [ config_h ] + files(
            'include/eventd-sd-module.h',
            'src/ssdp.c',
        ),
        c_args: [
            '-DG_LOG_DOMAIN="eventd-ssdp"',
        ],
        include_directories: include_directories('include'),
        dependencies: [
            gssdp,
            libeventd,
            libnkutils_uuid,
            gio,
            gobject,
            glib,
        ],
        name_prefix: '',
        install: true,
        install_dir: modules_install_dir,
    )
endif
