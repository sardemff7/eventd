# libcanberra plugin

canberra = dependency('libcanberra')

shared_library('canberra', config_h, files(
        'src/canberra.c',
    ),
    c_args: [
        '-DG_LOG_DOMAIN="eventd-canberra"',
    ],
    dependencies: [ canberra, libeventd_helpers, libeventd_plugin, libeventd, glib ],
    name_prefix: '',
    install: true,
    install_dir: plugins_install_dir,
)

man_pages += [ [ files('man/eventd-canberra.conf.xml'), 'eventd-canberra.conf.5' ] ]
docbook_conditions += 'enable_canberra'
