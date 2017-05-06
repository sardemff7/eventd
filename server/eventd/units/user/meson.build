systemduserunit_install_dir = get_option('systemduserunitdir')
if systemduserunit_install_dir == ''
    systemduserunit_install_dir = systemd.get_pkgconfig_variable('systemduserunitdir')
endif
eventd_user_units = []
foreach u : eventd_units
    configure_file(
            input: '@0@.in'.format(u),
            output: u,
            configuration: other_conf,
            install_dir: systemduserunit_install_dir,
        )
endforeach
