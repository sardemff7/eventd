AC_PREREQ([2.65])
AC_INIT([eventd],[0.1],[],[])

AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_SRCDIR([server/eventd/src/eventd.c])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([.aux])

AM_INIT_AUTOMAKE([1.11 subdir-objects dist-xz no-dist-gzip tar-ustar foreign])

AM_SILENT_RULES([yes])

AC_ARG_VAR([XSLTPROC], [The xsltproc executable])

AC_PROG_CC
AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_PROG_SED
AC_PROG_MKDIR_P
AC_DISABLE_STATIC
LT_INIT
PKG_PROG_PKG_CONFIG
AC_PATH_PROG([XSLTPROC], [xsltproc])
if test -z "$XSLTPROC"; then
    AC_MSG_ERROR([xsltproc not found])
fi

GETTEXT_PACKAGE=$PACKAGE_NAME
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Gettext Package])
AC_SUBST(GETTEXT_PACKAGE)
dnl Not yet useful
dnl IT_PROG_INTLTOOL([0.40.0])

AC_PROG_INSTALL

DEFAULT_BIND_PORT="7100"
DEFAULT_CONTROL_PORT="7101"
UNIX_SOCKET="sock"
AC_DEFINE_UNQUOTED([DEFAULT_BIND_PORT], [$DEFAULT_BIND_PORT], [Default TCP port to bind])
AC_DEFINE_UNQUOTED([DEFAULT_BIND_PORT_STR], ["$DEFAULT_BIND_PORT"], [Default TCP port to bind])
AC_DEFINE_UNQUOTED([DEFAULT_CONTROL_PORT], [$DEFAULT_CONTROL_PORT], [Default TCP port for the control interface])
AC_DEFINE_UNQUOTED([DEFAULT_CONTROL_PORT_STR], ["$DEFAULT_CONTROL_PORT"], [Default TCP port for the control interface])
AC_DEFINE_UNQUOTED([UNIX_SOCKET], ["$UNIX_SOCKET"], [Default UNIX socket name])
AC_SUBST(DEFAULT_BIND_PORT)
AC_SUBST(UNIX_SOCKET)

AC_DEFINE([EVENTD_EXPORT], [__attribute__((visibility("default")))], [Export symbol])

# Common dependencies
glib_min_major="2"
glib_min_minor="28"
glib_min_version="${glib_min_major}.${glib_min_minor}"
gio_min_version="$glib_min_version"

# Server dependencies
gmodule_min_version="$glib_min_version"
libpulse_min_version="0.9.15"
libnotify_min_version="0.5.0"
gtk_min_version="3.0"

# Client dependencies
vala_min_version="0.12"


AM_CFLAGS="-fvisibility=hidden -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers"
AM_LDFLAGS="-no-undefined -Wl,--no-undefined"
AM_VALA_CFLAGS="-fvisibility=default -Wno-unused"
AM_VALAFLAGS=""
AM_XSLTPROCFLAGS="--nonet --stringparam man.output.quietly 1 --stringparam funcsynopsis.style ansi"


if test -d .git; then
    PACKAGE_VERSION="${PACKAGE_VERSION} - `git describe --tags --always` (`git describe --tags --always --all | sed -e 's:heads/::'`)"
    AC_DEFINE_UNQUOTED([PACKAGE_VERSION], ["$PACKAGE_VERSION"], [Full version with git information])
fi

#
# Common dependencies
#
AC_CHECK_HEADERS([fcntl.h])

PKG_CHECK_MODULES(GLIB, [glib-2.0 >= $glib_min_version])
GLIB_VALAFLAGS="--pkg glib-2.0 --pkg glib-local-2.0"
AC_SUBST(GLIB_VALAFLAGS)
AC_DEFINE_UNQUOTED([GLIB_VERSION_MIN_REQUIRED], [(G_ENCODE_VERSION(${glib_min_major},${glib_min_minor}))], [The lower GLib version supported])

PKG_CHECK_MODULES(GOBJECT, [gobject-2.0])
GOBJECT_VALAFLAGS="--pkg gobject-2.0"
AC_SUBST(GOBJECT_VALAFLAGS)

PKG_CHECK_MODULES(GTHREAD, [gthread-2.0])

gio_unix=
gio_unix_vala=
PKG_CHECK_EXISTS([gio-unix-2.0 >= $gio_min_version], have_gio_unix=yes, have_gio_unix=no)
if test x$have_gio_unix = xyes; then
    gio_unix="gio-unix-2.0"
    gio_unix_vala="--pkg gio-unix-2.0 -D HAVE_GIO_UNIX"
    AC_DEFINE([HAVE_GIO_UNIX], [1], [UNIX-specific GIO support])
fi

PKG_CHECK_MODULES(GIO, [gio-2.0 >= $gio_min_version $gio_unix])
GIO_VALAFLAGS="--pkg gio-2.0 $gio_unix_vala"
AC_SUBST(gio_unix)
AC_SUBST(GIO_VALAFLAGS)

AC_ARG_ENABLE(avahi, AS_HELP_STRING([--disable-avahi], [Disable Avahi support]), [], enable_avahi=yes)
if test x$enable_avahi = xyes; then
    PKG_CHECK_MODULES(AVAHI, [avahi-client avahi-glib])
    AC_DEFINE([ENABLE_AVAHI], [1], [Enable Avahi support])
fi
AM_CONDITIONAL(ENABLE_AVAHI, test x$enable_avahi = xyes)


#
# Server and plugins
#

# Server
PKG_CHECK_MODULES(GMODULE, [gmodule-2.0 >= $gmodule_min_version])

AC_ARG_ENABLE(systemd, AS_HELP_STRING([--enable-systemd], [Enable systemd activation support]), [], enable_systemd=no)
AC_ARG_WITH(systemduserunitdir, AS_HELP_STRING([--with-systemduserunitdir=DIR], [Directory for systemd user unit files]), [], with_systemduserunitdir=yes)
if test x$enable_systemd = xyes; then
    case "$with_systemduserunitdir" in
        no|"") AC_MSG_ERROR([*** You must define systemduserunitdir when compiling systemd support]) ;;
        yes) systemduserunitdir="`$PKG_CONFIG --variable=systemduserunitdir systemd`" ;;
        *) systemduserunitdir="$with_systemduserunitdir" ;;
    esac
    PKG_CHECK_MODULES(SYSTEMD, [libsystemd-daemon])
    AC_DEFINE([ENABLE_SYSTEMD], [1], [Enable systemd activation support])
    AC_SUBST(systemduserunitdir)
fi
AM_CONDITIONAL(ENABLE_SYSTEMD, test x$enable_systemd = xyes)

AC_ARG_ENABLE(dbus, AS_HELP_STRING([--disable-dbus], [Disable D-Bus support (org.freedesktop.Notifications interface)]), [], enable_dbus=yes)
AC_ARG_WITH(dbussessionservicedir, AS_HELP_STRING([--with-dbussessionservicedir=DIR], [Directory for D-Bus session service files]), [], with_dbussessionservicedir=yes)
if test x$enable_dbus = xyes; then
    case "$with_dbussessionservicedir" in
        no|"") AC_MSG_ERROR([*** You must define dbussessionservicedir when compiling D-Bus support]) ;;
        yes) dbussessionservicedir="`$PKG_CONFIG --variable=session_bus_services_dir dbus-1`" ;;
        *) dbussessionservicedir="$with_dbussessionservicedir" ;;
    esac
    AC_SUBST(dbussessionservicedir)
fi
AM_CONDITIONAL(ENABLE_DBUS, test x$enable_dbus = xyes)

# sndfile plugin
AC_ARG_ENABLE(sndfile, AS_HELP_STRING([--disable-sndfile], [Disable libsndfile support]), [], enable_sndfile=yes)
if test x$enable_sndfile = xyes; then
    PKG_CHECK_MODULES(SNDFILE, [sndfile])
    PKG_CHECK_MODULES(PULSEAUDIO, [libpulse >= $libpulse_min_version libpulse-mainloop-glib])
fi
AM_CONDITIONAL(ENABLE_SNDFILE, test x$enable_sndfile = xyes)

# espeak plugin
AC_ARG_ENABLE(espeak, AS_HELP_STRING([--enable-espeak], [Enable eSpeak support]), [], enable_espeak=no)
if test x$enable_espeak = xyes; then
    ESPEAK_CFLAGS="-I/usr/include/espeak"
    ESPEAK_LIBS="-lespeak"
    AC_SEARCH_LIBS([espeak_Synth], [espeak],
        [
            AC_SUBST(ESPEAK_CFLAGS)
            AC_SUBST(ESPEAK_LIBS)
        ],
        [
            AC_MSG_ERROR([*** eSpeak support requested but library is missing])
        ])
fi
AM_CONDITIONAL(ENABLE_ESPEAK, test x$enable_espeak = xyes)

# libnotify plugin
AC_ARG_ENABLE(libnotify, AS_HELP_STRING([--enable-libnotify], [Enable libnotify support (client side)]), [], enable_libnotify=no)
if test x$enable_libnotify = xyes; then
    if test x$enable_dbus = xyes; then
        AC_MSG_ERROR([*** You can’t activate both org.freedesktop.Notifications server and libnotify client at the same time])
    fi
    PKG_CHECK_MODULES(NOTIFY, [libnotify >= $libnotify_min_version])
    PKG_CHECK_MODULES(GDK_PIXBUF, [gdk-pixbuf-2.0])
fi
AM_CONDITIONAL(ENABLE_NOTIFY, [test x$enable_libnotify = xyes])

# notification-daemon plugin
AC_ARG_ENABLE(notification-daemon, AS_HELP_STRING([--disable-notification-daemon], [Disable notification-daemon plugin]), [], enable_notification_daemon=yes)
if test x$enable_notification_daemon = xyes; then
    PKG_CHECK_MODULES(GDK_PIXBUF, [gdk-pixbuf-2.0])
    PKG_CHECK_MODULES(CAIRO, [cairo])
    PKG_CHECK_MODULES(PANGO, [pango pangocairo])
fi
AM_CONDITIONAL(ENABLE_NOTIFICATION_DAEMON, [test x$enable_notification_daemon = xyes])

# nd backends
AC_ARG_ENABLE(xcb, AS_HELP_STRING([--disable-xcb], [Disable XCB graphical backend]), [], enable_xcb=yes)
AC_ARG_ENABLE(linux-framebuffer, AS_HELP_STRING([--disable-linux-framebuffer], [Disable Linux framebuffer backend]), [], enable_linux_framebuffer=yes)

# Common
if test x$enable_xcb = xyes -o x$enable_linux_framebuffer = xyes; then
    if test x$enable_notification_daemon = xno; then
        AC_MSG_ERROR([*** XCB or Linux framebuffer backends required but not notification-daemon plugin])
    fi
fi

# XCB backend
if test x$enable_xcb = xyes; then
    PKG_CHECK_MODULES(XCB, [cairo-xcb xcb xcb-aux libxcb-glib xcb-shape])
fi
AM_CONDITIONAL(ENABLE_XCB, [test x$enable_xcb = xyes])

# Linux framebuffer backend
if test x$enable_linux_framebuffer = xyes; then
    AC_CHECK_HEADER([linux/fb.h])
fi
AM_CONDITIONAL(ENABLE_LINUX_FB, [test x$enable_linux_framebuffer = xyes])

#
# Client
#
AM_PROG_VALAC([$vala_min_version])



#
# Tests
#
dnl No additional dependencies for tests
dnl AC_ARG_ENABLE(tests, AS_HELP_STRING([--enable-tests], [Build tests]), [], enable_tests=yes)


# Debug flags
AC_ARG_ENABLE(debug, AS_HELP_STRING([--enable-debug], [Enable debug]), [], enable_debug=no)
if test x$enable_debug = xyes; then
    AC_DEFINE([DEBUG], [1], [Activate debug mode])
    AM_VALAFLAGS="${AM_VALAFLAGS} --debug -D DEBUG"
fi
AC_SUBST(AM_CFLAGS)
AC_SUBST(AM_LDFLAGS)
AC_SUBST(AM_VALA_CFLAGS)
AC_SUBST(AM_VALAFLAGS)
AC_SUBST(AM_XSLTPROCFLAGS)

AM_SUBST_NOTMAKE(LIBS)
AC_SUBST(shrext_cmds)

AC_CONFIG_FILES([
    server/libeventd-event/pkgconfig/libeventd-event.pc
    server/libeventd-event/vapi/libeventd-event.deps
    client/libeventc/pkgconfig/libeventc.pc
    client/libeventc/vapi/libeventc.deps
    Makefile
])
AC_OUTPUT

echo
echo
echo '    '${PACKAGE_NAME} ${PACKAGE_VERSION}
echo
echo '    'Core features:
echo '        'Avahi support: $enable_avahi
echo '        'systemd support: $enable_systemd
echo '        'UNIX sockets support: $have_gio_unix
echo
echo '    'Event collecting plugins:
echo '        'EVENT protocol: yes
echo '        'org.freedesktop.Notifications interface: $enable_dbus
echo
echo '    'Action plugins:
echo '        'relay: yes
echo '        'Sound files playing through libsndfile: $enable_sndfile
echo '        'Text-to-speech through eSpeak: $enable_espeak
echo '        'libnotify proxy: $enable_libnotify
echo '        'notification-daemon: $enable_notification_daemon
echo '          'Backends:
echo '            'XCB: $enable_xcb
echo '            'Linux framebuffer: $enable_linux_framebuffer
echo
dnl echo '    'Tests: $enable_tests
echo '    'Debug mode: $enable_debug
echo
echo
