# Server

eventd_c_args = [
    '-DG_LOG_DOMAIN="eventd"',
]
eventd_deps = [
    ws_module,
    libeventc,
    libeventd_plugin,
    libeventd_helpers,
    libeventd,
    libnkutils,
    gio,
    gmodule,
    gobject,
    glib
]
if get_option('enable-systemd')
    eventd_deps += dependency('libsystemd', version: '>= 209')
    eventd_c_args += '-DENABLE_SYSTEMD'
    docbook_conditions += 'enable_systemd'
endif

eventd = executable('eventd', [ config_h ] + files(
        'src/types.h',
        'src/config_.h',
        'src/config.c',
        'src/events.h',
        'src/events.c',
        'src/actions.h',
        'src/actions.c',
        'src/plugins.h',
        'src/plugins.c',
        'src/sd-modules.h',
        'src/sd-modules.c',
        'src/control.h',
        'src/control.c',
        'src/sockets.h',
        'src/sockets.c',
        'src/eventd.h',
        'src/eventd.c',
        'src/evp/evp.h',
        'src/evp/evp-internal.h',
        'src/evp/evp.c',
        'src/evp/client.h',
        'src/evp/client.c',
        'src/relay/relay.h',
        'src/relay/relay.c',
        'src/relay/server.h',
        'src/relay/server.c',
    ),
    c_args: eventd_c_args,
    dependencies: eventd_deps,
    include_directories: eventdctl_inc,
    install: true,
)


man_pages += [ [ files('man/eventd.xml'), 'eventd.1' ] ]
man_pages += [ [ files('man/eventd.conf.xml'), 'eventd.conf.5' ] ]

# Until pkgconfig module gains support for custom variable

configure_file(
    input: 'pkgconfig/eventd.pc.in',
    output: 'eventd.pc',
    configuration: pkgconfig_conf,
    install_dir: join_paths(get_option('datadir'), 'pkgconfig'),
)

if get_option('enable-systemd')
    systemd = dependency('systemd')
    eventd_units = [
        'eventd-control.socket',
        'eventd.socket',
        'eventd.service',
    ]

    subdir('units/user')
    subdir('units/system')
endif
