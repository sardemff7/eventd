# Basic CLI client

eventc = executable('eventc', [ config_h ] + files(
        'src/eventc.c',
    ),
    objects: libeventd_event_private,
    c_args: [
        '-DG_LOG_DOMAIN="eventc"',
    ],
    dependencies: [
        libeventc,
        libeventd,
        libnkutils_uuid,
        libnkutils,
        gio,
        gobject,
        glib,
    ],
    install: true
)
