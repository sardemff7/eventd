project('eventd', 'c',
    version: '0.23.90',
    meson_version: '>=0.39.1',
    license: [ 'GPL3+', 'LGPL3+', 'MIT' ],
    default_options: [
        'c_std=gnu11',
        'warning_level=2',
    ],
    subproject_dir: 'src',
)

eventd_version = meson.project_version()

git = find_program('git', required: false)
if git.found()
	git_version = run_command(git, '--git-dir', join_paths(meson.source_root(), '.git'), 'describe', '--tags', '--always', '--dirty')
	git_branch = run_command(git, '--git-dir', join_paths(meson.source_root(), '.git'), 'describe', '--tags', '--always', '--all')
	if git_version.returncode() == 0 and git_branch.returncode() == 0
		git_branch_parts = []
		foreach b : git_branch.stdout().strip().split('/')
			if b != 'heads'
				git_branch_parts += b
			endif
		endforeach
		eventd_version = '@0@ - @1@ (@2@)'.format(meson.project_version(), git_version.stdout().strip(), '/'.join(git_branch_parts))
	endif
endif

is_unix = host_machine.system() != 'windows'
is_windows = not is_unix
pkgconfig = import('pkgconfig')
gnome = import('gnome')

glib_min_major='2'
glib_min_minor='40'
glib_min_version='.'.join([glib_min_major, glib_min_minor])
glib = dependency('glib-2.0', version: '>= @0@'.format(glib_min_version))
gobject = dependency('gobject-2.0')
gio = dependency('gio-2.0')
if is_unix
    gio_platform = dependency('gio-unix-2.0')
else
    gio_platform = dependency('gio-windows-2.0')
endif
gmodule = dependency('gmodule-2.0')
headers = [
    'locale.h',
    'fcntl.h',
    'errno.h',
]
if is_unix
    headers += [
        'sys/socket.h',
    ]
else
    headers += [
        'windows.h',
    ]
endif
c_compiler = meson.get_compiler('c')
foreach h : headers
    if not c_compiler.has_header(h)
        error('Header @0@ was not found, but is required'.format(h))
    endif
endforeach

modules_install_dir = join_paths(get_option('libdir'), meson.project_name(), 'modules', meson.project_version())
plugins_install_dir = join_paths(get_option('libdir'), meson.project_name(), 'plugins')
events_install_dir = join_paths(get_option('datadir'), meson.project_name())

evp_service_name='evp'
evp_transport_name='tcp'
evp_service_type='_@0@._@1@'.format(evp_service_name, evp_transport_name)
evp_ssdp_ns_uuid='884987d8-ebb8-4e68-aa38-45a428eb86fa'
evp_ssdp_urn='urn:eventd-org:service:@0@:1'.format(evp_service_name)
evp_unix_socket=evp_service_name

header_conf = configuration_data()
pkgconfig_conf = configuration_data()
other_conf = configuration_data()
header_conf.set_quoted('PACKAGE_NAME', meson.project_name())
header_conf.set_quoted('EVENTD_VERSION', eventd_version)
header_conf.set_quoted('MODULES_VERSION', meson.project_version())
header_conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
pkgconfig_conf.set('VERSION', meson.project_version())
other_conf.set('PACKAGE_NAME', meson.project_name())
other_conf.set('VERSION', meson.project_version())
other_conf.set('EVENTD_VERSION', eventd_version)

if is_unix
    header_conf.set('EVENTD_EXPORT', '__attribute__((visibility("default")))')
else
    header_conf.set('EVENTD_EXPORT', '__declspec(dllexport)')
    header_conf.set('_WIN32_WINNT', '_WIN32_WINNT_WIN7')
    header_conf.set('NTDDI_VERSION', 'NTDDI_WIN7')
endif
header_conf.set('GLIB_VERSION_MIN_REQUIRED', '(G_ENCODE_VERSION(@0@,@1@))'.format(glib_min_major, glib_min_minor))
header_conf.set('G_LOG_USE_STRUCTURED', true)

header_conf.set_quoted('EVENTD_SYSCONFDIR', join_paths(get_option('prefix'), get_option('sysconfdir')))
header_conf.set_quoted('EVENTD_BINDIR', join_paths(get_option('prefix'), get_option('bindir')))
header_conf.set_quoted('EVENTD_LIBDIR', join_paths(get_option('prefix'), get_option('libdir')))
header_conf.set_quoted('EVENTD_DATADIR', join_paths(get_option('prefix'), get_option('datadir')))
header_conf.set_quoted('EVENTD_LOCALEDIR', join_paths(get_option('prefix'), get_option('localedir')))
pkgconfig_conf.set('prefix', get_option('prefix'))
pkgconfig_conf.set('datadir', join_paths('${prefix}', get_option('datadir')))
other_conf.set('prefix', get_option('prefix'))
other_conf.set('bindir', join_paths(get_option('prefix'), get_option('bindir')))
other_conf.set('datadir', join_paths(get_option('prefix'), get_option('datadir')))

header_conf.set_quoted('EVP_SERVICE_NAME', evp_service_name)
header_conf.set_quoted('EVP_TRANSPORT_NAME', evp_transport_name)
header_conf.set_quoted('EVP_SERVICE_TYPE', evp_service_type)
header_conf.set_quoted('EVP_SSDP_NS_UUID', evp_ssdp_ns_uuid)
header_conf.set_quoted('EVP_SSDP_URN', evp_ssdp_urn)
header_conf.set_quoted('EVP_UNIX_SOCKET', evp_unix_socket)
pkgconfig_conf.set('EVP_SSDP_NS_UUID', evp_ssdp_ns_uuid)
pkgconfig_conf.set('EVP_SSDP_URN', evp_ssdp_urn)
other_conf.set('EVP_SERVICE_NAME', evp_service_name)
other_conf.set('EVP_TRANSPORT_NAME', evp_transport_name)
other_conf.set('EVP_SERVICE_TYPE', evp_service_type)
other_conf.set('EVP_SSDP_NS_UUID', evp_ssdp_ns_uuid)
other_conf.set('EVP_SSDP_URN', evp_ssdp_urn)
other_conf.set('EVP_UNIX_SOCKET', evp_unix_socket)

header_conf.set('EVENTD_DEBUG', get_option('enable-debug'))
header_conf.set('DISABLE_IPV6', not get_option('enable-ipv6'))

config_h = configure_file(output: 'config.h', configuration: header_conf)
config_ent = configure_file(input: 'src/config.ent.in', output: 'config.ent', configuration: other_conf)

add_project_arguments(
    '-fvisibility=hidden',
    '-I@0@'.format(meson.build_root()),
    '-I@0@/src'.format(meson.source_root()),
    language: 'c'
)
flags = [
    '-Wformat=2',
    '-Wno-unused-parameter',
]
foreach f : flags
    if c_compiler.has_argument(f)
        add_project_arguments(f, language: 'c')
    endif
endforeach

man_pages = []
docbook_conditions = [ 'installation' ]
if is_unix
    docbook_conditions += [ 'ev_os_unix' ]
else
    docbook_conditions += [ 'ev_os_win' ]
endif

nk_modules = [
    'uuid=true',
    'enum=true',
    'token=true',
    'colour=true',
    'xdg-theme=true',
]
nk = subproject('libnkutils', default_options: nk_modules)
nk_options = nk.get_variable('nk_options')
foreach o : nk_modules + nk_options
    if not nk_modules.contains(o) or not nk_options.contains(o)
        error('You must not change libnkutils options @0@ != @1@'.format('|'.join(nk_modules), '|'.join(nk_options)))
    endif
endforeach
libnkutils = nk.get_variable('libnkutils')
nkutils_xsltpath = nk.get_variable('nkutils_xsltpath')
nkutils_manfiles = nk.get_variable('nkutils_manfiles')
docbook_conditions += nk.get_variable('nkutils_docbook_conditions')

subdir('server/libeventd')
subdir('server/libeventd-helpers')
subdir('server/libeventd-plugin')

ws_module = declare_dependency(
    include_directories: include_directories('server/modules/include'),
    sources: files('server/modules/src/ws-load.c'),
    dependencies: libeventd_helpers,
)

subdir('client/libeventc')
subdir('client/libeventc-light')
subdir('client/eventc')
subdir('server/eventdctl')
subdir('server/modules')
subdir('server/eventd')

subdir('plugins/exec')
subdir('plugins/file')
subdir('plugins/fdo-notifications')
if get_option('enable-notification-daemon')
    subdir('plugins/nd')
endif
if get_option('enable-im')
    subdir('plugins/im')
endif
if get_option('enable-sound')
    subdir('plugins/sound')
endif
if get_option('enable-tts')
    subdir('plugins/tts')
endif
if get_option('enable-webhook')
    subdir('plugins/webhook')
endif
if get_option('enable-libnotify')
    subdir('plugins/notify')
endif
if get_option('enable-libcanberra')
    subdir('plugins/canberra')
endif

# Integration testing
subdir('plugins/test-plugin')
subdir('server/libeventd-test')
subdir('server/eventd/tests')
subdir('client/libeventc/tests')


xsltproc = [
    find_program('xsltproc'),
    '-o', '@OUTDIR@',
    '--nonet', '--xinclude',
    '--stringparam', 'man.output.quietly', '1',
    '--stringparam', 'funcsynopsis.style', 'ansi',
    '--stringparam', 'profile.condition', ';'.join(docbook_conditions),
]
foreach p : [
    join_paths(meson.current_source_dir(), 'src'),
    meson.current_build_dir(),
    nkutils_xsltpath,
]
    xsltproc += [ '--path', p ]
endforeach
xsltproc += [
    'http://docbook.sourceforge.net/release/xsl/current/manpages/profile-docbook.xsl',
    '@INPUT@'
]
man_input = []
man_output = []
foreach m : man_pages
    s = m[1].split('.')
    section = s[s.length() - 1]
    custom_target(m[1],
        input: m[0],
        output: m[1],
        command: xsltproc,
        depend_files: [ config_ent ],
        build_by_default: true,
        install: true,
        install_dir: join_paths(get_option('mandir'), section)
    )
endforeach
