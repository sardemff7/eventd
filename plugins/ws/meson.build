# webhook plugin

libsoup = dependency('libsoup-3.0', required: get_option('websocket'))

test_plugin_path += [ meson.current_build_dir() ]
shared_library('ws', config_h, files(
        'src/ws.c',
        'src/ws-client.c',
    ),
    c_args: [
        '-DG_LOG_DOMAIN="eventd-ws"',
    ],
    dependencies: [ libsoup, libeventd_helpers, libeventd_plugin, libeventd, libnkutils, glib ],
    name_prefix: '',
    install: true,
    install_dir: plugins_install_dir,
)

man_pages += [ [ files('man/eventd-ws.conf.xml'), 'eventd-ws.conf.5' ] ]
