# Basic CLI client

eventc = executable('eventc', [
        config_h,
        'src/eventc.c',
    ],
    objects: libeventd_lib.extract_objects('src/event-private.c'),
    c_args: [
        '-DG_LOG_DOMAIN="eventc"',
    ],
    dependencies: [
        libeventc,
        libeventd,
        libnkutils,
        gio,
        gobject,
        glib,
    ],
    install: true
)
