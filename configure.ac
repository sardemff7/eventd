m4_define([eventd_version_major], [0])
m4_define([eventd_version_minor], [9])
m4_define([eventd_version_patch], [])
m4_define([eventd_homepage], [http://www.eventd.org])
m4_define([eventd_bug_report_page], [https://github.com/sardemff7/eventd/issues])
AC_PREREQ([2.65])
AC_INIT([eventd],eventd_version_major.eventd_version_minor[]eventd_version_patch,eventd_bug_report_page,[],eventd_homepage)

AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_SRCDIR([server/eventd/src/eventd.c])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([.aux])

AM_INIT_AUTOMAKE([1.14 foreign subdir-objects parallel-tests dist-xz no-dist-gzip tar-ustar])
AM_SILENT_RULES([yes])

AH_BOTTOM([#include <src/config-version.h>])
AM_CONDITIONAL([EVENTD_USE_GIT_VERSION], [test -d .git])


AC_ARG_VAR([XSLTPROC], [The xsltproc executable])

AC_PROG_CC
AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_PROG_AWK
AC_PROG_SED
AC_PROG_MKDIR_P
LT_INIT([disable-static pic-only])
PKG_PROG_PKG_CONFIG
AC_PATH_PROG([XSLTPROC], [xsltproc])
if test -z "${XSLTPROC}"; then
    AC_MSG_ERROR([xsltproc not found])
fi
AC_PROG_INSTALL

GETTEXT_PACKAGE=${PACKAGE_NAME}
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["${GETTEXT_PACKAGE}"], [Gettext Package])
AC_SUBST([GETTEXT_PACKAGE])
dnl IT_PROG_INTLTOOL([0.40.0])
dnl AM_GLIB_GNU_GETTEXT


EVP_SERVICE_NAME="evp"
EVP_TRANSPORT_NAME="tcp"
EVP_SERVICE_TYPE="_${EVP_SERVICE_NAME}._${EVP_TRANSPORT_NAME}"
EVP_UNIX_SOCKET="${EVP_SERVICE_NAME}"
AC_DEFINE_UNQUOTED([EVP_SERVICE_NAME],   ["${EVP_SERVICE_NAME}"],   [EvP service name])
AC_DEFINE_UNQUOTED([EVP_TRANSPORT_NAME], ["${EVP_TRANSPORT_NAME}"], [EvP transport name])
AC_DEFINE_UNQUOTED([EVP_SERVICE_TYPE],   ["${EVP_SERVICE_TYPE}"],   [EvP service type (for SRV DNS records)])
AC_DEFINE_UNQUOTED([EVP_UNIX_SOCKET],    ["${EVP_UNIX_SOCKET}"],   [EvP UNIX socket name])
AC_SUBST([EVP_SERVICE_NAME])
AC_SUBST([EVP_TRANSPORT_NAME])
AC_SUBST([EVP_SERVICE_TYPE])
AC_SUBST([EVP_UNIX_SOCKET])

AC_DEFINE([EVENTD_EXPORT], [__attribute__((visibility("default")))], [Export symbol])

# Common dependencies
glib_min_major="2"
glib_min_minor="40"
glib_min_version="${glib_min_major}.${glib_min_minor}"
gio_min_version="${glib_min_version}"
gobject_introspection_min_version="1.42.0"

# Server dependencies
gmodule_min_version="${glib_min_version}"


AM_CFLAGS="-fvisibility=hidden -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers"
AM_CPPFLAGS=""
AM_LDFLAGS="-no-undefined"
AM_XSLTPROCFLAGS="--nonet --xinclude --stringparam man.output.quietly 1 --stringparam funcsynopsis.style ansi"
AM_DOCBOOK_CONDITIONS="installation"


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

# aclocal m4 files
aclocaldir='${datadir}/aclocal'
AC_SUBST([aclocaldir])

# VAPI directory
vapidir='${datadir}/vala/vapi'
AC_SUBST([vapidir])

# eventd specific dirs
pluginsdir='${pkglibdir}/plugins'
eventdir='${pkgdatadir}'
fdonotificationscapabilitiesdir='${pluginsdir}/fdo-notifications'
AC_SUBST([pluginsdir])
AC_SUBST([eventdir])
AC_SUBST([fdonotificationscapabilitiesdir])

dnl GLIB_DEFINE_LOCALEDIR([EVENTD_LOCALEDIR])
GW_INIT
NK_INIT([enum token colour/alpha colour/double])

#
# Common dependencies
#
AC_CHECK_HEADERS([locale.h fcntl.h errno.h])

PKG_CHECK_MODULES([UUID], [uuid])

PKG_CHECK_MODULES([GLIB], [glib-2.0 >= ${glib_min_version}])
AC_DEFINE_UNQUOTED([GLIB_VERSION_MIN_REQUIRED], [(G_ENCODE_VERSION(${glib_min_major},${glib_min_minor}))], [The lower GLib version supported])

PKG_CHECK_MODULES([GOBJECT], [gobject-2.0])

PKG_CHECK_MODULES([GTHREAD], [gthread-2.0])

gio_unix=
PKG_CHECK_EXISTS([gio-unix-2.0 >= ${gio_min_version}], have_gio_unix=yes, have_gio_unix=no)
if test x${have_gio_unix} = xyes; then
    gio_unix="gio-unix-2.0"
    AC_DEFINE([HAVE_GIO_UNIX], [1], [UNIX-specific GIO support])
    AM_DOCBOOK_CONDITIONS="${AM_DOCBOOK_CONDITIONS};have_gio_unix"
fi

PKG_CHECK_MODULES([GIO], [gio-2.0 >= ${gio_min_version} ${gio_unix}])
AC_SUBST([gio_unix])

AC_ARG_ENABLE([avahi], AS_HELP_STRING([--disable-avahi], [Disable Avahi support]), [], [enable_avahi=yes])
if test x${enable_avahi} = xyes; then
    PKG_CHECK_MODULES([AVAHI], [avahi-client avahi-glib])
    AC_DEFINE([ENABLE_AVAHI], [1], [Enable Avahi support])
    AM_DOCBOOK_CONDITIONS="${AM_DOCBOOK_CONDITIONS};enable_avahi"
fi
AM_CONDITIONAL([ENABLE_AVAHI], [test x${enable_avahi} = xyes])

m4_ifdef([GOBJECT_INTROSPECTION_CHECK], [
    GOBJECT_INTROSPECTION_CHECK([${gobject_introspection_min_version}])
    if test x${enable_introspection} = xyes; then
        AC_ARG_WITH([girdir], AS_HELP_STRING([--with-girdir=DIR], [Directory for gobject introspection gir files]), [], [with_girdir=yes])
        AC_ARG_WITH([typelibdir], AS_HELP_STRING([--with-typelibdir=DIR], [Directory for gobject introspection typelib files]), [], [with_typelibdir=yes])
        case "${with_girdir}" in
            no|"") AC_MSG_ERROR([*** You must define girdir when compiling gobject introspection support]) ;;
            yes) girdir="`${PKG_CONFIG} --variable=girdir gobject-introspection-1.0`" ;;
            *) girdir="${with_girdir}" ;;
        esac
        case "${with_typelibdir}" in
            no|"") AC_MSG_ERROR([*** You must define typelibdir when compiling gobject introspection support]) ;;
            yes) typelibdir="`${PKG_CONFIG} --variable=typelibdir gobject-introspection-1.0`" ;;
            *) typelibdir="${with_typelibdir}" ;;
        esac
    fi
], [
    AM_CONDITIONAL([HAVE_INTROSPECTION], [false])
])
AC_SUBST([girdir])
AC_SUBST([typelibdir])

# JSON protocol support
AC_ARG_ENABLE([json], AS_HELP_STRING([--enable-json], [Enable JSON protocol support]), [], [enable_json=no])
if test x${enable_json} = xyes; then
    PKG_CHECK_MODULES([YAJL], [yajl >= 2])
    AC_DEFINE([ENABLE_JSON], [1], [Enable JSON protocol support])
    AM_DOCBOOK_CONDITIONS="${AM_DOCBOOK_CONDITIONS};enable_json"
fi
AM_CONDITIONAL([ENABLE_JSON], [test x${enable_json} = xyes])

#
# Server and plugins
#

# Server
PKG_CHECK_MODULES([GMODULE], [gmodule-2.0 >= ${gmodule_min_version}])

AC_ARG_ENABLE([systemd], AS_HELP_STRING([--enable-systemd], [Enable systemd activation support]), [], [enable_systemd=no])
AC_ARG_WITH([systemduserunitdir], AS_HELP_STRING([--with-systemduserunitdir=DIR], [Directory for systemd user unit files]), [], [with_systemduserunitdir=yes])
if test x${enable_systemd} = xyes; then
    case "${with_systemduserunitdir}" in
        no|"") AC_MSG_ERROR([*** You must define systemduserunitdir when compiling systemd support]) ;;
        yes) systemduserunitdir="`${PKG_CONFIG} --variable=systemduserunitdir systemd`" ;;
        *) systemduserunitdir="${with_systemduserunitdir}" ;;
    esac
    AC_CHECK_HEADERS([sys/socket.h])
    PKG_CHECK_MODULES([SYSTEMD], [libsystemd >= 209],, [PKG_CHECK_MODULES([SYSTEMD], [libsystemd-daemon])])
    AC_DEFINE([ENABLE_SYSTEMD], [1], [Enable systemd activation support])
    AC_SUBST([systemduserunitdir])
fi
AM_CONDITIONAL([ENABLE_SYSTEMD], [test x${enable_systemd} = xyes])

AC_ARG_WITH([dbussessionservicedir], AS_HELP_STRING([--with-dbussessionservicedir=DIR], [Directory for D-Bus session service files]), [], [with_dbussessionservicedir=yes])
case "${with_dbussessionservicedir}" in
    no|"") AC_MSG_ERROR([*** You must define dbussessionservicedir when compiling D-Bus support]) ;;
    yes) dbussessionservicedir="`${PKG_CONFIG} --variable=session_bus_services_dir dbus-1`" ;;
    *) dbussessionservicedir="${with_dbussessionservicedir}" ;;
esac
AC_SUBST([dbussessionservicedir])

# http plugin
libsoup_min_version="2.50"
AC_ARG_ENABLE([http], AS_HELP_STRING([--enable-http], [Enable HTTP plugin]), [], [enable_http=no])
if test x${enable_http} = xyes; then
    if test x${enable_json} = xno; then
        AC_MSG_ERROR([*** JSON protocol support is required for HTTP plugin])
    fi
    PKG_CHECK_MODULES([LIBSOUP], [libsoup-2.4 >= ${libsoup_min_version}])
    AM_DOCBOOK_CONDITIONS="${AM_DOCBOOK_CONDITIONS};enable_http"
fi
AM_CONDITIONAL([ENABLE_HTTP], [test x${enable_http} = xyes])

# notification-daemon plugin
AC_ARG_ENABLE([notification-daemon], AS_HELP_STRING([--disable-notification-daemon], [Disable notification-daemon plugin]), [], [enable_notification_daemon=yes])
if test x${enable_notification_daemon} = xyes; then
    AC_CHECK_HEADERS([math.h])
    PKG_CHECK_MODULES([CAIRO], [cairo])
    PKG_CHECK_MODULES([PANGO], [pango pangocairo])
    PKG_CHECK_MODULES([GDK_PIXBUF], [gdk-pixbuf-2.0])
    AM_DOCBOOK_CONDITIONS="${AM_DOCBOOK_CONDITIONS};enable_notification_daemon"
fi
AM_CONDITIONAL([ENABLE_NOTIFICATION_DAEMON], [test x${enable_notification_daemon} = xyes])

# nd backends
AC_ARG_ENABLE([xcb], AS_HELP_STRING([--disable-xcb], [Disable XCB graphical backend]), [], [enable_xcb=yes])
AC_ARG_ENABLE([linux-framebuffer], AS_HELP_STRING([--disable-linux-framebuffer], [Disable Linux framebuffer backend]), [], [enable_linux_framebuffer=yes])

# Common
if test x${enable_xcb} = xyes -o x${enable_linux_framebuffer} = xyes; then
    if test x${enable_notification_daemon} = xno; then
        AC_MSG_ERROR([*** XCB or Linux framebuffer backends required but not notification-daemon plugin])
    fi
fi

# XCB backend
if test x${enable_xcb} = xyes; then
    GW_CHECK_XCB([cairo-xcb xcb xcb-aux xcb-randr xcb-shape])
    AC_DEFINE([ENABLE_XCB], [1], [Enable XCB backend for notification-daemon plugin])
    AM_DOCBOOK_CONDITIONS="${AM_DOCBOOK_CONDITIONS};enable_xcb"
fi
AM_CONDITIONAL([ENABLE_XCB], [test x${enable_xcb} = xyes])

# Linux framebuffer backend
if test x${enable_linux_framebuffer} = xyes; then
    AC_CHECK_HEADER([linux/fb.h])
    AM_DOCBOOK_CONDITIONS="${AM_DOCBOOK_CONDITIONS};enable_linux_framebuffer"
fi
AM_CONDITIONAL([ENABLE_LINUX_FB], [test x${enable_linux_framebuffer} = xyes])

# IM plugin
AC_ARG_ENABLE([im], AS_HELP_STRING([--disable-im], [Disable IM support through libpurple]), [], [enable_im=yes])
if test x${enable_im} = xyes; then
    PKG_CHECK_MODULES([PURPLE], [purple])
    # No existing libpurple allows us to avoid that for now
    AC_DEFINE([PURPLE_NEEDS_GLOBAL_LOADING], [1], [Needs to load libpurple symbols in the global namespace])
    AM_DOCBOOK_CONDITIONS="${AM_DOCBOOK_CONDITIONS};enable_im"
fi
AM_CONDITIONAL([ENABLE_IM], [test x${enable_im} = xyes])

# sound plugin
libpulse_min_version="0.9.15"
AC_ARG_ENABLE([sound], AS_HELP_STRING([--disable-sound], [Disable sound support through libsndfile]), [], [enable_sound=yes])
if test x${enable_sound} = xyes; then
    PKG_CHECK_MODULES([SNDFILE], [sndfile])
    PKG_CHECK_MODULES([PULSEAUDIO], [libpulse >= ${libpulse_min_version} libpulse-mainloop-glib])
    AC_DEFINE([ENABLE_SOUND], [1], [Enable sound plugin])
    AM_DOCBOOK_CONDITIONS="${AM_DOCBOOK_CONDITIONS};enable_sound"
fi
AM_CONDITIONAL([ENABLE_SOUND], [test x${enable_sound} = xyes])

# TTS plugin
AC_ARG_ENABLE([tts], AS_HELP_STRING([--enable-tts], [Enable Text-to-speech support through eSpeak]), [], [enable_tts=no])
if test x${enable_tts} = xyes; then
    ESPEAK_CFLAGS='-I${includedir}/espeak'
    ESPEAK_LIBS='-lespeak'
    AC_SEARCH_LIBS([espeak_Synth], [espeak],
        [
            AC_SUBST([ESPEAK_CFLAGS])
            AC_SUBST([ESPEAK_LIBS])
        ],
        [
            AC_MSG_ERROR([*** TTS support requested but eSpeak library is missing])
        ])
    AM_DOCBOOK_CONDITIONS="${AM_DOCBOOK_CONDITIONS};enable_tts"
fi
AM_CONDITIONAL([ENABLE_TTS], [test x${enable_tts} = xyes])

# libnotify plugin
AC_ARG_ENABLE([libnotify], AS_HELP_STRING([--disable-libnotify], [Disable libnotify support (client side)]), [], [enable_libnotify=yes])
if test x${enable_libnotify} = xyes; then
    PKG_CHECK_MODULES([GDK_PIXBUF], [gdk-pixbuf-2.0])
    AM_DOCBOOK_CONDITIONS="${AM_DOCBOOK_CONDITIONS};enable_notify"
fi
AM_CONDITIONAL([ENABLE_NOTIFY], [test x${enable_libnotify} = xyes])

# libcanberra plugin
AC_ARG_ENABLE([libcanberra], AS_HELP_STRING([--enable-libcanberra], [Enable libcanberra support]), [], [enable_libcanberra=no])
if test x${enable_libcanberra} = xyes; then
    PKG_CHECK_MODULES([CANBERRA], [libcanberra])
    AM_DOCBOOK_CONDITIONS="${AM_DOCBOOK_CONDITIONS};enable_canberra"
fi
AM_CONDITIONAL([ENABLE_CANBERRA], [test x${enable_libcanberra} = xyes])



#
# Tests
#
dnl No additional dependencies for tests
dnl AC_ARG_ENABLE([tests], AS_HELP_STRING([--enable-tests], [Build tests]), [], [enable_tests=yes])

# Debug flags
AC_ARG_ENABLE([debug], AS_HELP_STRING([--enable-debug], [Enable debug]), [], [enable_debug=no])
if test x${enable_debug} = xyes; then
    AC_DEFINE([EVENTD_DEBUG], [1], [Activate debug mode])
fi

AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_CPPFLAGS])
AC_SUBST([AM_LDFLAGS])
AC_SUBST([AM_XSLTPROCFLAGS])
AC_SUBST([AM_DOCBOOK_CONDITIONS])

AM_SUBST_NOTMAKE([LIBS])
AC_SUBST([shrext_cmds])

AC_CONFIG_FILES([
    server/libeventd/pkgconfig/libeventd.pc
    server/libeventd/pkgconfig/libeventd-protocol-json.pc
    server/libeventd/vapi/libeventd.deps
    server/eventd/pkgconfig/eventd.pc
    server/libeventd-plugin/pkgconfig/libeventd-plugin.pc
    client/libeventc/pkgconfig/libeventc.pc
    client/libeventc/vapi/libeventc.deps
    src/config.ent
    Makefile
])
AC_OUTPUT

AC_MSG_RESULT([
    ${PACKAGE_NAME} ${PACKAGE_VERSION}

    Core features:
        Avahi support: ${enable_avahi}
        systemd support: ${enable_systemd}
        UNIX sockets support: ${have_gio_unix}
        GObject introspection: ${enable_introspection}
        EVENT JSON protocol library: ${enable_json}

    Event collection plugins:
        EVENT protocol: yes
        org.freedesktop.Notifications interface: yes
        EVENT JSON protocol over WebSocket: ${enable_http}

    Action plugins:
        EVENT protocol relay: yes
        exec plugin: yes
        notification-daemon: ${enable_notification_daemon}
            Backends:
                XCB: ${enable_xcb}
                Linux framebuffer: ${enable_linux_framebuffer}
        IM support through libpurple: ${enable_im}
        Sound files playing through libsndfile: ${enable_sound}
        Text-to-speech through eSpeak: ${enable_tts}
        libnotify proxy: ${enable_libnotify}
        libcanberra: ${enable_libcanberra}

dnl     Tests: ${enable_tests}
    Debug mode: ${enable_debug}
])
