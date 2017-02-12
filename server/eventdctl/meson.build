# Server control utility

eventdctl_inc = include_directories('include')
eventdctl = executable('eventdctl', [
        'include/eventdctl.h',
        'src/eventdctl.c',
    ],
    c_args: [
        '-DG_LOG_DOMAIN="eventdctl"',
    ],
    dependencies: [
        libeventd_plugin,
        libeventd,
        gio_platform,
        gio,
        glib,
    ],
    include_directories: eventdctl_inc,
    install: true
)

man_pages += [ [ files('man/eventdctl.xml'), 'eventdctl.1' ] ]
