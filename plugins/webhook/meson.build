# webhook plugin

libsoup = dependency('libsoup-2.4', version: '>= 2.42')

shared_library('webhook', files(
        'src/webhook.c',
    ),
    c_args: [
        '-DG_LOG_DOMAIN="eventd-webhook"',
    ],
    dependencies: [ libsoup, libeventd_helpers, libeventd_plugin, libeventd, glib ],
    name_prefix: '',
    install: true,
    install_dir: plugins_install_dir,
)

man_pages += [ [ files('man/eventd-webhook.conf.xml'), 'eventd-webhook.conf.5' ] ]
