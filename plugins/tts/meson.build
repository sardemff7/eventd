# tts plugin

speechd = dependency('speech-dispatcher')

shared_library('tts', files(
        'src/tts.c',
    ),
    c_args: [
        '-DG_LOG_DOMAIN="eventd-tts"',
    ],
    dependencies: [ speechd, libeventd_helpers, libeventd_plugin, libeventd, glib ],
    name_prefix: '',
    install: true,
    install_dir: plugins_install_dir,
)

man_pages += [ [ files('man/eventd-tts.conf.xml'), 'eventd-tts.conf.5' ] ]
docbook_conditions += 'enable_tts'
