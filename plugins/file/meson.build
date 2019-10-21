# file plugin

shared_library('file', config_h, files(
        'src/file.c',
    ),
    c_args: [
        '-DG_LOG_DOMAIN="eventd-file"',
    ],
    dependencies: [ libeventd_helpers, libeventd_plugin, libeventd, gio, glib ],
    name_prefix: '',
    install: true,
    install_dir: plugins_install_dir,
)

man_pages += [ [ files('man/eventd-file.conf.xml'), 'eventd-file.conf.5' ] ]
