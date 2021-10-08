# IM plugin

libpurple = dependency('purple')

shared_library('im', config_h, files(
        'src/im.c',
        'src/ops.h',
        'src/ops.c',
    ),
    c_args: [
        '-DG_LOG_DOMAIN="eventd-im"',
    ],
    dependencies: [ libpurple, libeventd_helpers, libeventd_plugin, libeventd, gmodule, glib ],
    name_prefix: '',
    install: true,
    install_dir: plugins_install_dir,
)

man_pages += [ [ files('man/eventdctl-im.xml'), 'eventdctl-im.1' ] ]
man_pages += [ [ files('man/eventd-im.conf.xml'), 'eventd-im.conf.5' ] ]
docbook_conditions += 'enable_im'
