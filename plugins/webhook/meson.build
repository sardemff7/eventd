# webhook plugin

libsoup = dependency('libsoup-3.0')

shared_library('webhook', config_h, files(
        'src/webhook.c',
    ),
    c_args: [
        '-DG_LOG_DOMAIN="eventd-webhook"',
    ],
    dependencies: [ libsoup, libeventd_helpers, libeventd_plugin, libeventd, libnkutils, glib ],
    name_prefix: '',
    install: true,
    install_dir: plugins_install_dir,
)

man_pages += [ [ files('man/eventd-webhook.conf.xml'), 'eventd-webhook.conf.5' ] ]
