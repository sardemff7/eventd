# test plugin

test_plugin_path = meson.current_build_dir()
test_plugin = shared_module('test-plugin', [ config_h ] + files(
        'src/test-plugin.c',
    ),
    c_args: [
        '-DG_LOG_DOMAIN="eventd-test-plugin"',
    ],
    objects: libeventd_event_private,
    dependencies: [ libeventd_plugin, libeventd, gio, gobject, libnkutils_uuid, libnkutils, glib ],
    name_prefix: '',
)
