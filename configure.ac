m4_define([eventd_version_major], [0])
m4_define([eventd_version_minor], [1])
m4_define([eventd_version_patch], [])
AC_PREREQ([2.65])
AC_INIT([eventd],eventd_version_major.eventd_version_minor[]eventd_version_patch,[],[])

AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_SRCDIR([server/eventd/src/eventd.c])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([.aux])

AM_INIT_AUTOMAKE([1.11 foreign subdir-objects parallel-tests dist-xz no-dist-gzip tar-ustar])

AM_SILENT_RULES([yes])

AC_ARG_VAR([XSLTPROC], [The xsltproc executable])

AC_PROG_CC
AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_PROG_SED
AC_PROG_MKDIR_P
LT_INIT([disable-static pic-only])
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

DOCBOOK_CONDITIONS="installation"


AM_CFLAGS="-fvisibility=hidden -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers"
AM_CPPFLAGS=""
AM_LDFLAGS="-Wl,--no-undefined"
AM_XSLTPROCFLAGS="--nonet --xinclude --stringparam man.output.quietly 1 --stringparam funcsynopsis.style ansi"


if test -d .git; then
    PACKAGE_VERSION="${PACKAGE_VERSION} - `git describe --tags --always` (`git describe --tags --always --all | sed -e 's:heads/::'`)"
    AC_DEFINE_UNQUOTED([PACKAGE_VERSION], ["$PACKAGE_VERSION"], [Full version with git information])
fi


#
# Directories
#

# package-specific dirs
pkglibdir='${libdir}/'${PACKAGE}
pkgincludedir='${includedir}/'${PACKAGE}
pkgdatadir='${datadir}/'${PACKAGE}
AC_SUBST([pkglibdir])
AC_SUBST([pkgincludedir])
AC_SUBST([pkgdatadir])

# pkg-config files
PKG_INSTALLDIR
PKG_NOARCH_INSTALLDIR

# eventd specific dirs
pluginsdir='${pkglibdir}/plugins'
eventdir='${pkgdatadir}'
dbuscapabilitiesdir="${pluginsdir}/dbus"
AC_SUBST([pluginsdir])
AC_SUBST([eventdir])
AC_SUBST([dbuscapabilitiesdir])


#
# Common dependencies
#
AC_CHECK_HEADERS([fcntl.h errno.h])

PKG_CHECK_MODULES(GLIB, [glib-2.0 >= $glib_min_version])
AC_DEFINE_UNQUOTED([GLIB_VERSION_MIN_REQUIRED], [(G_ENCODE_VERSION(${glib_min_major},${glib_min_minor}))], [The lower GLib version supported])

PKG_CHECK_MODULES(GOBJECT, [gobject-2.0])

PKG_CHECK_MODULES(GTHREAD, [gthread-2.0])

gio_unix=
PKG_CHECK_EXISTS([gio-unix-2.0 >= $gio_min_version], have_gio_unix=yes, have_gio_unix=no)
if test x$have_gio_unix = xyes; then
    gio_unix="gio-unix-2.0"
    AC_DEFINE([HAVE_GIO_UNIX], [1], [UNIX-specific GIO support])
    DOCBOOK_CONDITIONS="${DOCBOOK_CONDITIONS};have_gio_unix"
fi

PKG_CHECK_MODULES(GIO, [gio-2.0 >= $gio_min_version $gio_unix])
AC_SUBST(gio_unix)

AC_ARG_ENABLE(avahi, AS_HELP_STRING([--disable-avahi], [Disable Avahi support]), [], enable_avahi=yes)
if test x$enable_avahi = xyes; then
    PKG_CHECK_MODULES(AVAHI, [avahi-client avahi-glib])
    AC_DEFINE([ENABLE_AVAHI], [1], [Enable Avahi support])
    DOCBOOK_CONDITIONS="${DOCBOOK_CONDITIONS};enable_avahi"
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
    AC_CHECK_HEADERS([sys/socket.h])
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
    DOCBOOK_CONDITIONS="${DOCBOOK_CONDITIONS};enable_dbus"
    AC_CONFIG_FILES([
        plugins/dbus/pkgconfig/eventd-dbus.pc
    ])
fi
AM_CONDITIONAL(ENABLE_DBUS, test x$enable_dbus = xyes)

# notification-daemon plugin
AC_ARG_ENABLE(notification-daemon, AS_HELP_STRING([--disable-notification-daemon], [Disable notification-daemon plugin]), [], enable_notification_daemon=yes)
if test x$enable_notification_daemon = xyes; then
    AC_CHECK_HEADERS([math.h])
    PKG_CHECK_MODULES(CAIRO, [cairo])
    PKG_CHECK_MODULES(PANGO, [pango pangocairo])
    DOCBOOK_CONDITIONS="${DOCBOOK_CONDITIONS};enable_notification_daemon"
fi
AM_CONDITIONAL(ENABLE_NOTIFICATION_DAEMON, [test x$enable_notification_daemon = xyes])

AC_ARG_ENABLE(gdk-pixbuf, AS_HELP_STRING([--disable-gdk-pixbuf], [Disable image and icon support for notification-daemon through gdk-pixbuf]), [], enable_gdk_pixbuf=yes)
if test x$enable_gdk_pixbuf = xyes; then
    if test x$enable_notification_daemon = xno; then
        AC_MSG_ERROR([*** image and icon support required but not notification-daemon plugin])
    fi
    PKG_CHECK_MODULES(GDK_PIXBUF, [gdk-pixbuf-2.0])
    AC_DEFINE([ENABLE_GDK_PIXBUF], [1], [Enable image and icon support for notification-daemon through gdk-pixbuf])
    DOCBOOK_CONDITIONS="${DOCBOOK_CONDITIONS};enable_gdk_pixbuf"
fi
AM_CONDITIONAL(ENABLE_GDK_PIXBUF, [test x$enable_gdk_pixbuf = xyes])

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
    PKG_CHECK_MODULES(XCB, [cairo-xcb xcb xcb-aux libxcb-glib xcb-randr xcb-shape])
    AC_DEFINE([ENABLE_XCB], [1], [Enable XCB backend for notification-daemon plugin])
    DOCBOOK_CONDITIONS="${DOCBOOK_CONDITIONS};enable_xcb"
    if test x$enable_dbus = xyes; then
        DOCBOOK_CONDITIONS="${DOCBOOK_CONDITIONS};enable_full_notification_daemon"
    fi
fi
AM_CONDITIONAL(ENABLE_XCB, [test x$enable_xcb = xyes])

# Linux framebuffer backend
if test x$enable_linux_framebuffer = xyes; then
    AC_CHECK_HEADER([linux/fb.h])
    DOCBOOK_CONDITIONS="${DOCBOOK_CONDITIONS};enable_linux_framebuffer"
fi
AM_CONDITIONAL(ENABLE_LINUX_FB, [test x$enable_linux_framebuffer = xyes])

# IM plugin
AC_ARG_ENABLE(im, AS_HELP_STRING([--disable-im], [Disable IM support through libpurple]), [], enable_im=yes)
if test x$enable_im = xyes; then
    PKG_CHECK_MODULES(PURPLE, [purple])
    # No existing libpurple allows us to avoid that for now
    AC_DEFINE([PURPLE_NEEDS_GLOBAL_LOADING], [1], [Needs to load libpurple symbols in the global namespace])
    DOCBOOK_CONDITIONS="${DOCBOOK_CONDITIONS};enable_im"
fi
AM_CONDITIONAL(ENABLE_IM, test x$enable_im = xyes)

# sndfile plugin
AC_ARG_ENABLE(sndfile, AS_HELP_STRING([--disable-sndfile], [Disable libsndfile support]), [], enable_sndfile=yes)
if test x$enable_sndfile = xyes; then
    PKG_CHECK_MODULES(SNDFILE, [sndfile])
    PKG_CHECK_MODULES(PULSEAUDIO, [libpulse >= $libpulse_min_version libpulse-mainloop-glib])
    DOCBOOK_CONDITIONS="${DOCBOOK_CONDITIONS};enable_sndfile"
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
    DOCBOOK_CONDITIONS="${DOCBOOK_CONDITIONS};enable_espeak"
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
    DOCBOOK_CONDITIONS="${DOCBOOK_CONDITIONS};enable_notify"
fi
AM_CONDITIONAL(ENABLE_NOTIFY, [test x$enable_libnotify = xyes])



#
# Tests
#
dnl No additional dependencies for tests
dnl AC_ARG_ENABLE(tests, AS_HELP_STRING([--enable-tests], [Build tests]), [], enable_tests=yes)

# Debug flags
AC_ARG_ENABLE(debug, AS_HELP_STRING([--enable-debug], [Enable debug]), [], enable_debug=no)
if test x$enable_debug = xyes; then
    AC_DEFINE([DEBUG], [1], [Activate debug mode])
fi

AM_XSLTPROCFLAGS="${AM_XSLTPROCFLAGS} --stringparam profile.condition '${DOCBOOK_CONDITIONS}'"

AC_SUBST(AM_CFLAGS)
AC_SUBST(AM_CPPFLAGS)
AC_SUBST(AM_LDFLAGS)
AC_SUBST(AM_XSLTPROCFLAGS)

AM_SUBST_NOTMAKE(LIBS)
AC_SUBST(shrext_cmds)

AC_CONFIG_FILES([
    server/libeventd-event/pkgconfig/libeventd-event.pc
    server/libeventd-event/vapi/libeventd-event.deps
    server/eventd/pkgconfig/eventd.pc
    server/libeventd-plugin/pkgconfig/libeventd-plugin.pc
    client/libeventc/pkgconfig/libeventc.pc
    client/libeventc/vapi/libeventc.deps
    Makefile
])
AC_OUTPUT

AC_MSG_RESULT([
    ${PACKAGE_NAME} ${PACKAGE_VERSION}

    Core features:
        Avahi support: $enable_avahi
        systemd support: $enable_systemd
        UNIX sockets support: $have_gio_unix

    Event collection plugins:
        EVENT protocol: yes
        org.freedesktop.Notifications interface: $enable_dbus

    Action plugins:
        EVENT protocol relay: yes
        exec plugin: yes
        notification-daemon: $enable_notification_daemon
            Icon support: $enable_gdk_pixbuf
            Backends:
                XCB: $enable_xcb
                Linux framebuffer: $enable_linux_framebuffer
        IM support through libpurple: $enable_im
        Sound files playing through libsndfile: $enable_sndfile
        Text-to-speech through eSpeak: $enable_espeak
        libnotify proxy: $enable_libnotify

dnl     Tests: $enable_tests
    Debug mode: $enable_debug
])
