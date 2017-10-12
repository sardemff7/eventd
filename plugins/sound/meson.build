# sound plugin

sndfile = dependency('sndfile')
pulseaudio = [ dependency('libpulse', version: '>= 0.9.15'), dependency('libpulse-mainloop-glib') ]

shared_library('sound', [ config_h ] + files(
        'src/sound.c',
        'src/pulseaudio.h',
        'src/pulseaudio.c',
    ),
    c_args: [
        '-DG_LOG_DOMAIN="eventd-sound"',
    ],
    dependencies: [ sndfile, pulseaudio, libeventd_helpers, libeventd_plugin, libeventd, libnkutils, glib ],
    name_prefix: '',
    install: true,
    install_dir: plugins_install_dir,
)

man_pages += [ [ files('man/eventd-sound.conf.xml'), 'eventd-sound.conf.5' ] ]
docbook_conditions += 'enable_sound'
