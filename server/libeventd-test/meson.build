# Testing helper library

libeventd_test_inc = include_directories('include')
libeventd_test_dep = [
    libeventc,
    libeventd,
    gobject,
    glib,
]
libeventd_test_lib = library('eventd-test', config_h, files(
        'src/libeventc-test.c',
        'src/libtest.c',
    ),
    c_args: [
        '-DG_LOG_DOMAIN="libeventd-test"',
        '-fvisibility=default',
        '-DSRC_DIR="@0@"'.format(meson.project_source_root()),
        '-DEVENTDCTL_PATH="@0@"'.format(eventdctl.full_path()),
        '-DEVENTD_PATH="@0@"'.format(eventd.full_path()),
        '-DTEST_PLUGIN_PATH="@0@"'.format(test_plugin_path),
        '-DMODULES_BUILD_DIR="@0@"'.format(modules_build_dir),
    ],
    dependencies: libeventd_test_dep,
    include_directories: libeventd_test_inc,
)

libeventd_test = declare_dependency(link_with: libeventd_test_lib, include_directories: libeventd_test_inc, dependencies: libeventd_test_dep)
