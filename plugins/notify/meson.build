# notify plugin

gdk_pixbuf = dependency('gdk-pixbuf-2.0')

shared_library('notify', [
        config_h,
        'src/notify.c',
        '../nd/src/pixbuf.c',
    ],
    c_args: [
        '-DG_LOG_DOMAIN="eventd-notify"',
    ],
    dependencies: [ gdk_pixbuf, libeventd_helpers, libeventd_plugin, libeventd, libnkutils, gio, glib ],
    name_prefix: '',
    install: true,
    install_dir: plugins_install_dir,
)

man_pages += [ [ files('man/eventd-notify.conf.xml'), 'eventd-notify.conf.5' ] ]
docbook_conditions += 'enable_notify'
