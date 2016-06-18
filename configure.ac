m4_define([eventd_version_major], [0])
m4_define([eventd_version_minor], [18])
m4_define([eventd_version_patch], [90])
m4_define([eventd_homepage], [https://www.eventd.org])
m4_define([eventd_bug_report_page], [https://github.com/sardemff7/eventd/issues])
AC_PREREQ([2.65])
AC_INIT([eventd],eventd_version_major.eventd_version_minor.eventd_version_patch,eventd_bug_report_page,[],eventd_homepage)

AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_SRCDIR([server/eventd/src/eventd.c])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([.aux])

AM_INIT_AUTOMAKE([1.14 foreign subdir-objects parallel-tests dist-xz no-dist-gzip tar-ustar])
AM_SILENT_RULES([yes])

AM_CONDITIONAL([EVENTD_USE_GIT_VERSION], [test -d "${srcdir}"/.git])


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
EVP_SSDP_NS_UUID="884987d8-ebb8-4e68-aa38-45a428eb86fa"
EVP_SSDP_URN="urn:eventd-org:service:${EVP_SERVICE_NAME}:1"
EVP_UNIX_SOCKET="${EVP_SERVICE_NAME}"
AC_DEFINE_UNQUOTED([EVP_SERVICE_NAME],   ["${EVP_SERVICE_NAME}"],   [EvP service name])
AC_DEFINE_UNQUOTED([EVP_TRANSPORT_NAME], ["${EVP_TRANSPORT_NAME}"], [EvP transport name])
AC_DEFINE_UNQUOTED([EVP_SERVICE_TYPE],   ["${EVP_SERVICE_TYPE}"],   [EvP service type (for SRV DNS records)])
AC_DEFINE_UNQUOTED([EVP_SSDP_NS_UUID],   ["${EVP_SSDP_NS_UUID}"],   [EvP SSDP namespace UUID])
AC_DEFINE_UNQUOTED([EVP_SSDP_URN],       ["${EVP_SSDP_URN}"],       [EvP SSDP service URN])
AC_DEFINE_UNQUOTED([EVP_UNIX_SOCKET],    ["${EVP_UNIX_SOCKET}"],   [EvP UNIX socket name])
AC_SUBST([EVP_SERVICE_NAME])
AC_SUBST([EVP_TRANSPORT_NAME])
AC_SUBST([EVP_SERVICE_TYPE])
AC_SUBST([EVP_SSDP_NS_UUID])
AC_SUBST([EVP_SSDP_URN])
AC_SUBST([EVP_UNIX_SOCKET])

AC_DEFINE([EVENTD_EXPORT], [__attribute__((visibility("default")))], [Export symbol])

AC_CANONICAL_HOST
ev_os_unix=no
ev_os_win=no
case "${host_os}" in
    mingw*)
        ev_os_win=yes
        pkg_config_extra_args="--dont-define-prefix"
    ;;
    *)
        ev_os_unix=yes
        pkg_config_extra_args=
    ;;
esac
AM_CONDITIONAL([EV_OS_UNIX], [test x${ev_os_unix} = xyes])
AM_CONDITIONAL([EV_OS_WIN], [test x${ev_os_win} = xyes])

# Common dependencies
glib_min_major="2"
glib_min_minor="40"
glib_min_version="${glib_min_major}.${glib_min_minor}"
gio_min_version="${glib_min_version}"
gobject_introspection_min_version="1.42.0"
soup_min_version="2.50"

# Server dependencies
gmodule_min_version="${glib_min_version}"


AM_CFLAGS="-fvisibility=hidden -Wall -Wextra -Wformat=2 -Wno-unused-parameter -Wno-missing-field-initializers"
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
modulesdir='${pkglibdir}/modules/${PACKAGE_VERSION}'
eventdir='${pkgdatadir}'
AC_SUBST([pluginsdir])
AC_SUBST([modulesdir])
AC_SUBST([eventdir])

dnl GLIB_DEFINE_LOCALEDIR([EVENTD_LOCALEDIR])
GW_INIT
NK_INIT([uuid enum token colour/alpha colour/double xdg-theme])

#
# Common dependencies
#
AC_CHECK_HEADERS([locale.h fcntl.h errno.h])
AC_FUNC_MMAP

PKG_CHECK_MODULES([GLIB], [glib-2.0 >= ${glib_min_version}])
AC_DEFINE_UNQUOTED([GLIB_VERSION_MIN_REQUIRED], [(G_ENCODE_VERSION(${glib_min_major},${glib_min_minor}))], [The lower GLib version supported])

PKG_CHECK_MODULES([GOBJECT], [gobject-2.0])

PKG_CHECK_MODULES([GTHREAD], [gthread-2.0])

gio_unix=
if test x${ev_os_unix} = xyes; then
    gio_unix="gio-unix-2.0"
fi

PKG_CHECK_MODULES([GIO], [gio-2.0 >= ${gio_min_version} ${gio_unix}])
AC_SUBST([gio_unix])

EV_CHECK_FEATURE([websocket], [WebSocket support], [SOUP], [libsoup-2.4 >= ${soup_min_version}])
EV_CHECK_FEATURE([dns-sd], [DNS-SD support], [AVAHI], [avahi-client avahi-glib])
EV_CHECK_FEATURE([ssdp], [SSDP support], [GSSDP], [gssdp-1.0])

m4_ifdef([GOBJECT_INTROSPECTION_CHECK], [
    GOBJECT_INTROSPECTION_CHECK([${gobject_introspection_min_version}])
    if test x${enable_introspection} = xyes; then
        AC_ARG_WITH([girdir], AS_HELP_STRING([--with-girdir=DIR], [Directory for gobject introspection gir files]), [], [with_girdir=yes])
        AC_ARG_WITH([typelibdir], AS_HELP_STRING([--with-typelibdir=DIR], [Directory for gobject introspection typelib files]), [], [with_typelibdir=yes])
        case "${with_girdir}" in
            no|"") AC_MSG_ERROR([*** You must define girdir when compiling gobject introspection support]) ;;
            yes) girdir="`${PKG_CONFIG} ${pkg_config_extra_args} --variable=girdir gobject-introspection-1.0`" ;;
            *) girdir="${with_girdir}" ;;
        esac
        case "${with_typelibdir}" in
            no|"") AC_MSG_ERROR([*** You must define typelibdir when compiling gobject introspection support]) ;;
            yes) typelibdir="`${PKG_CONFIG} ${pkg_config_extra_args} --variable=typelibdir gobject-introspection-1.0`" ;;
            *) typelibdir="${with_typelibdir}" ;;
        esac
    fi
], [
    AM_CONDITIONAL([HAVE_INTROSPECTION], [false])
])
AC_SUBST([girdir])
AC_SUBST([typelibdir])

# libeventc-light
if test x${ev_os_unix} = xyes; then
    AC_CHECK_HEADERS([sys/socket.h sys/un.h netinet/in.h])
else
    AC_CHECK_HEADERS([winsock2.h windows.h])
fi

#
# Server and plugins
#

# Server
PKG_CHECK_MODULES([GMODULE], [gmodule-2.0 >= ${gmodule_min_version}])

AC_ARG_ENABLE([systemd], AS_HELP_STRING([--enable-systemd], [Enable systemd activation support]), [], [enable_systemd=no])
AC_ARG_WITH([systemduserunitdir], AS_HELP_STRING([--with-systemduserunitdir=DIR], [Directory for systemd user unit files]), [], [with_systemduserunitdir=yes])
AC_ARG_WITH([systemdsystemunitdir], AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd system unit files]), [], [with_systemdsystemunitdir=yes])
if test x${enable_systemd} = xyes; then
    case "${with_systemduserunitdir}" in
        no|"") AC_MSG_ERROR([*** You must define systemduserunitdir when compiling systemd support]) ;;
        yes) systemduserunitdir="`${PKG_CONFIG} ${pkg_config_extra_args} --variable=systemduserunitdir systemd`" ;;
        *) systemduserunitdir="${with_systemduserunitdir}" ;;
    esac
    case "${with_systemdsystemunitdir}" in
        no|"") AC_MSG_ERROR([*** You must define systemdsystemunitdir when compiling systemd support]) ;;
        yes) systemdsystemunitdir="`${PKG_CONFIG} ${pkg_config_extra_args} --variable=systemdsystemunitdir systemd`" ;;
        *) systemdsystemunitdir="${with_systemdsystemunitdir}" ;;
    esac
    AC_CHECK_HEADERS([sys/socket.h])
    PKG_CHECK_MODULES([SYSTEMD], [libsystemd >= 209],, [PKG_CHECK_MODULES([SYSTEMD], [libsystemd-daemon])])
    AC_DEFINE([ENABLE_SYSTEMD], [1], [Enable systemd activation support])
    AM_DOCBOOK_CONDITIONS="${AM_DOCBOOK_CONDITIONS};enable_systemd"
    AC_SUBST([systemduserunitdir])
    AC_SUBST([systemdsystemunitdir])
fi
AM_CONDITIONAL([ENABLE_SYSTEMD], [test x${enable_systemd} = xyes])

AC_ARG_WITH([dbussessionservicedir], AS_HELP_STRING([--with-dbussessionservicedir=DIR], [Directory for D-Bus session service files]), [], [with_dbussessionservicedir=yes])
if test x${ev_os_unix} = xyes; then
    case "${with_dbussessionservicedir}" in
        no|"") AC_MSG_ERROR([*** You must define dbussessionservicedir when compiling D-Bus support]) ;;
        yes) dbussessionservicedir="`${PKG_CONFIG} ${pkg_config_extra_args} --variable=session_bus_services_dir dbus-1`" ;;
        *) dbussessionservicedir="${with_dbussessionservicedir}" ;;
    esac
    AC_SUBST([dbussessionservicedir])
fi

# notification-daemon plugin
cairo_min_version="1.14"
AC_ARG_ENABLE([notification-daemon], AS_HELP_STRING([--disable-notification-daemon], [Disable notification-daemon plugin]), [], [enable_notification_daemon=yes])
if test x${enable_notification_daemon} = xyes; then
    EV_CHECK_LIB([MATH], [math.h], [m], [sqrt], [], [AC_MSG_ERROR([*** notification daemon plugin requested but math library is missing])])
    PKG_CHECK_MODULES([CAIRO], [cairo >= ${cairo_min_version}])
    PKG_CHECK_MODULES([PANGO], [pango pangocairo])
    PKG_CHECK_MODULES([GDK_PIXBUF], [gdk-pixbuf-2.0])
    AM_DOCBOOK_CONDITIONS="${AM_DOCBOOK_CONDITIONS};enable_notification_daemon"
else
    enabled_nd_backends=" no"
fi
AM_CONDITIONAL([ENABLE_NOTIFICATION_DAEMON], [test x${enable_notification_daemon} = xyes])

# nd backends
AC_ARG_ENABLE([nd-wayland], AS_HELP_STRING([--disable-nd-wayland], [Disable Wayland graphical backend]), [], [enable_nd_wayland=${ev_os_unix}])
AC_ARG_ENABLE([nd-xcb], AS_HELP_STRING([--disable-nd-xcb], [Disable XCB graphical backend]), [], [enable_nd_xcb=${ev_os_unix}])
AC_ARG_ENABLE([nd-fbdev], AS_HELP_STRING([--enable-nd-fbdev], [Enable fbdev backend]), [], [enable_nd_fbdev=no])
AC_ARG_ENABLE([nd-windows], AS_HELP_STRING([--enable-nd-windows], [Enable Windows backend]), [], [enable_nd_windows=${ev_os_win}])

# Common
if test x${enable_nd_wayland} = xyes -o x${enable_nd_xcb} = xyes -o x${enable_nd_fbdev} = xyes -o x${enable_nd_windows} = xyes; then
    if test x${enable_notification_daemon} = xno; then
        AC_MSG_ERROR([*** Wayland, XCB, fbdev or Windows backends required but not notification-daemon plugin])
    fi
fi
if test x${enable_notification_daemon} = xyes; then
    if test x${enable_nd_wayland} = xno -a x${enable_nd_xcb} = xno -a x${enable_nd_fbdev} = xno -a x${enable_nd_windows} = xno; then
        AC_MSG_ERROR([*** notification-daemon plugin required but no backend selected])
    fi
fi

# Wayland backend
wayland_min_version=1.9.91
wna_min_version=3
if test x${enable_nd_wayland} = xyes; then
    AC_CHECK_HEADER([sys/mman.h])
    if test x${ac_cv_func_mmap_fixed_mapped} = xno; then
        AC_MSG_ERROR([*** mmap() function is required for the Wayland backend of the notification-daemon plugin])
    fi
    GW_CHECK_WAYLAND([wayland-client >= ${wayland_min_version} wayland-cursor wayland-notification-area >= ${wna_min_version}])
    AC_ARG_VAR([WAYLAND_SCANNER], [Wayland XML scanner executable])
    AC_PATH_PROG([WAYLAND_SCANNER], [wayland-scanner])
    if test -z "${WAYLAND_SCANNER}"; then
        AC_MSG_ERROR([wayland-scanner not found])
    fi
    wnadir=`${PKG_CONFIG} --variable=pkgdatadir wayland-notification-area`
    AC_SUBST([wnadir])
    AC_DEFINE([ENABLE_ND_WAYLAND], [1], [nd Wayland backend])
    DOCBOOK_CONDITIONS="${DOCBOOK_CONDITIONS};enable_nd_wayland"
    enabled_nd_backends="${enabled_nd_backends}, Wayland"
fi
AM_CONDITIONAL([ENABLE_ND_WAYLAND], [test x${enable_nd_wayland} = xyes])

# XCB backend
if test x${enable_nd_xcb} = xyes; then
    GW_CHECK_XCB([cairo-xcb xcb xcb-aux xcb-randr xcb-ewmh xcb-xfixes xcb-shape])
    AC_DEFINE([ENABLE_ND_XCB], [1], [nd XCB backend])
    AM_DOCBOOK_CONDITIONS="${AM_DOCBOOK_CONDITIONS};enable_nd_xcb"
    enabled_nd_backends="${enabled_nd_backends}, XCB"
fi
AM_CONDITIONAL([ENABLE_ND_XCB], [test x${enable_nd_xcb} = xyes])

# fbdev backend
if test x${enable_nd_fbdev} = xyes; then
    AC_CHECK_HEADER([linux/fb.h])
    AC_DEFINE([ENABLE_ND_FBDEV], [1], [nd fbdev backend])
    AM_DOCBOOK_CONDITIONS="${AM_DOCBOOK_CONDITIONS};enable_nd_fbdev"
    enabled_nd_backends="${enabled_nd_backends}, fbdev"
fi
AM_CONDITIONAL([ENABLE_ND_FBDEV], [test x${enable_nd_fbdev} = xyes])

# Windows API backend
if test x${enable_nd_windows} = xyes; then
    if test x${ev_os_win} != xyes; then
        AC_MSG_ERROR([*** Windows backends required but not compiling for MinGW])
    fi
    GW_CHECK_WIN([cairo-win32])
    AC_DEFINE([ENABLE_ND_WIN], [1], [nd Windows backend])
    AM_DOCBOOK_CONDITIONS="${AM_DOCBOOK_CONDITIONS};enable_nd_windows"
    enabled_nd_backends="${enabled_nd_backends}, Windows"
fi
AM_CONDITIONAL([ENABLE_ND_WINDOWS], [test x${enable_nd_windows} = xyes])

# IM plugin
AC_ARG_ENABLE([im], AS_HELP_STRING([--disable-im], [Disable IM support through libpurple]), [], [enable_im=${ev_os_unix}])
if test x${enable_im} = xyes; then
    PKG_CHECK_MODULES([PURPLE], [purple])
    # No existing libpurple allows us to avoid that for now
    AC_DEFINE([PURPLE_NEEDS_GLOBAL_LOADING], [1], [Needs to load libpurple symbols in the global namespace])
    AM_DOCBOOK_CONDITIONS="${AM_DOCBOOK_CONDITIONS};enable_im"
fi
AM_CONDITIONAL([ENABLE_IM], [test x${enable_im} = xyes])

# sound plugin
libpulse_min_version="0.9.15"
AC_ARG_ENABLE([sound], AS_HELP_STRING([--disable-sound], [Disable sound support through libsndfile]), [], [enable_sound=${ev_os_unix}])
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
    EV_CHECK_LIB([ESPEAK], [espeak/speak_lib.h], [espeak], [espeak_Synth], [], [AC_MSG_ERROR([*** TTS support requested but eSpeak library is missing])])
    AM_DOCBOOK_CONDITIONS="${AM_DOCBOOK_CONDITIONS};enable_tts"
fi
AM_CONDITIONAL([ENABLE_TTS], [test x${enable_tts} = xyes])

# libnotify plugin
AC_ARG_ENABLE([libnotify], AS_HELP_STRING([--disable-libnotify], [Disable libnotify support (client side)]), [], [enable_libnotify=${ev_os_unix}])
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



# Debug
AC_ARG_ENABLE([debug], AS_HELP_STRING([--disable-debug], [Disable debug output]), [], [enable_debug=yes])
if test x${enable_debug} = xyes; then
    AC_DEFINE([EVENTD_DEBUG], [1], [Activate debug mode])
fi

AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_CPPFLAGS])
AC_SUBST([AM_LDFLAGS])
AC_SUBST([AM_XSLTPROCFLAGS])
AC_SUBST([AM_DOCBOOK_CONDITIONS])

AM_SUBST_NOTMAKE([LIBS])
AC_SUBST([LT_OBJDIR], [${lt_cv_objdir}])

AC_CONFIG_FILES([
    server/libeventd/pkgconfig/libeventd.pc
    server/libeventd/vapi/libeventd.deps
    server/eventd/pkgconfig/eventd.pc
    server/libeventd-plugin/pkgconfig/libeventd-plugin.pc
    client/libeventc/pkgconfig/libeventc.pc
    client/libeventc/vapi/libeventc.deps
    client/libeventc-light/pkgconfig/libeventc-light.pc
    Makefile
])
AC_OUTPUT

AC_MSG_RESULT([
    Core features:
        DNS-SD support: ${enable_dns_sd}
        SSDP support: ${enable_ssdp}
        systemd support: ${enable_systemd}
        UNIX sockets support: ${ev_os_unix}
        WebSocket support: ${enable_websocket}
        GObject introspection: ${enable_introspection}

    Event collection plugins:
        org.freedesktop.Notifications interface: yes

    Action plugins:
        exec plugin: yes
        file writing plugin: yes
        notification-daemon:${enabled_nd_backends#,}
        IM support through libpurple: ${enable_im}
        Sound files playing through libsndfile: ${enable_sound}
        Text-to-speech through eSpeak: ${enable_tts}
        libnotify proxy: ${enable_libnotify}
        libcanberra: ${enable_libcanberra}

    Debug output: ${enable_debug}
])
