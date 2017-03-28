systemdsystemunit_install_dir = get_option('systemdsystemunitdir')
if systemdsystemunit_install_dir == ''
    systemdsystemunit_install_dir = systemd.get_pkgconfig_variable('systemdsystemunitdir')
endif

eventd_system_units = []
foreach u : eventd_units
    eventd_system_units += configure_file(
            input: '@0@.in'.format(u),
            output: u,
            configuration: other_conf,
        )
endforeach

install_data(eventd_system_units, install_dir: systemdsystemunit_install_dir)
