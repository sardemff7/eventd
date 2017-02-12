# Basic CLI client

eventc = executable('eventc', [
        config_h,
        'src/eventc.c',
    ],
    c_args: [
        '-DG_LOG_DOMAIN="eventc"',
    ],
    dependencies: [
        libeventc,
        libeventd,
        gio,
        gobject,
        glib,
    ],
    install: true
)
