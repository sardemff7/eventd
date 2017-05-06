# Freedesktop.org notifications event collection plugin

shared_library('fdo-notifications', [
        config_h,
        'src/fdo-notifications.c',
    ],
    c_args: [
        '-DG_LOG_DOMAIN="eventd-fdo-notifications"',
    ],
    dependencies: [ libeventd_plugin, libeventd, gio, glib ],
    name_prefix: '',
    install: true,
    install_dir: plugins_install_dir,
)

dbussessionservice_install_dir = get_option('dbussessionservicedir')
if dbussessionservice_install_dir == ''
    dbus = dependency('dbus-1')
    dbussessionservice_install_dir = dbus.get_pkgconfig_variable('session_bus_services_dir')
endif

configure_file(
    input: 'services/org.eventd.fdo-notifications.service.in',
    output: 'org.eventd.fdo-notifications.service',
    configuration: other_conf,
    install_dir: dbussessionservice_install_dir,
)

install_data([
        'events/notification.event',
        'events/notification.action',
    ],
    install_dir: events_install_dir,
)
