# exec plugin

shared_library('exec', [
        config_h,
        'src/exec.c',
    ],
    c_args: [
        '-DG_LOG_DOMAIN="eventd-exec"',
    ],
    dependencies: [ libeventd_helpers, libeventd_plugin, libeventd, glib ],
    name_prefix: '',
    install: true,
    install_dir: plugins_install_dir,
)

man_pages += [ [ files('man/eventd-exec.conf.xml'), 'eventd-exec.conf.5' ] ]
