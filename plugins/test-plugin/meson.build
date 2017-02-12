# test plugin

test_plugin = shared_module('test-plugin', [
        config_h,
        'src/test-plugin.c',
    ],
    c_args: [
        '-DG_LOG_DOMAIN="eventd-test-plugin"',
    ],
    objects: libeventd_event_private,
    dependencies: [ libeventd_plugin, libeventd, gio, gobject, libnkutils, glib ],
    name_prefix: '',
)
