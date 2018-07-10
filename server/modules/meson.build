modules_build_dir = meson.current_build_dir()

libsoup = dependency('libsoup-2.4', version: '>=2.50', required: false)
module_enable_websocket = get_option('websocket')
if module_enable_websocket == 'auto' and libsoup.found()
    module_enable_websocket = 'true'
endif
if module_enable_websocket == 'true'
    if not libsoup.found()
        error('WebSocket module requested but libsoup not found')
    endif
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

avahi = dependency('avahi-client', required: false)
avahi_glib = dependency('avahi-glib', required: false)
module_enable_dns_sd = get_option('dns-sd')
if module_enable_dns_sd == 'auto' and avahi.found() and avahi_glib.found()
    module_enable_dns_sd = 'true'
endif
if module_enable_dns_sd == 'true'
    if not avahi.found() or not avahi_glib.found()
        error('DNS-SD module requested but avahi not found')
    endif
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

gssdp = dependency('gssdp-1.0', required: false)
module_enable_ssdp = get_option('ssdp')
if module_enable_ssdp == 'auto' and gssdp.found()
    module_enable_ssdp = 'true'
endif
if module_enable_ssdp == 'true'
    if not gssdp.found()
        error('SSDP module requested but GSSDP not found')
    endif
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
