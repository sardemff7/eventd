AC_PREREQ([2.65])
AC_INIT([eventd],[0.1],[],[])

AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_SRCDIR([src/])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([.aux])

AM_INIT_AUTOMAKE([1.11 subdir-objects dist-xz no-dist-gzip tar-ustar foreign])

AM_SILENT_RULES([yes])

AC_PROG_CC
AM_PROG_CC_C_O
AC_DISABLE_STATIC
LT_INIT

GETTEXT_PACKAGE=$PACKAGE_NAME
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Gettext Package])
AC_SUBST(GETTEXT_PACKAGE)
dnl Not yet useful
dnl IT_PROG_INTLTOOL([0.40.0])

AC_PROG_INSTALL

DEFAULT_BIND_PORT="7100"
UNIX_SOCKET="sock"
AC_DEFINE_UNQUOTED([DEFAULT_BIND_PORT], [$DEFAULT_BIND_PORT], [Default TCP port to bind])
AC_DEFINE_UNQUOTED([UNIX_SOCKET], ["$UNIX_SOCKET"], [Default UNIX socket name])

# Common dependencies
glib_min_version="2.28.0"
gio_min_version="$glib_min_version"

# Server dependencies
gmodule_min_version="$glib_min_version"
libpulse_min_version="0.9.15"
libnotify_min_version="0.5.0"
gtk_min_version="3.0"

# Client dependencies
vala_min_version="0.12"


AM_CFLAGS="-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers"
AM_LDFLAGS=""
AM_VALA_CFLAGS="-Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable"
AM_VALAFLAGS=""


#
# Common dependencies
#
PKG_CHECK_MODULES(GLIB, [glib-2.0 >= $glib_min_version])
GLIB_VALAFLAGS="--pkg glib-2.0 --pkg glib-local-2.0"
AC_SUBST(GLIB_VALAFLAGS)

PKG_CHECK_MODULES(GOBJECT, [gobject-2.0])
GOBJECT_VALAFLAGS="--pkg gobject-2.0"
AC_SUBST(GOBJECT_VALAFLAGS)

gio_unix=
gio_unix_vala=
AC_ARG_ENABLE(gio-unix, AS_HELP_STRING([--disable-gio-unix], [Disable UNIX-specific GIO support]), enable_gio_unix=$enableval, enable_gio_unix=yes)
if test x$enable_gio_unix = xyes; then
    gio_unix="gio-unix-2.0"
    gio_unix_vala="--pkg gio-unix-2.0 -D ENABLE_GIO_UNIX"
    AC_DEFINE([ENABLE_GIO_UNIX], [1], [Enable UNIX-specific GIO support])
fi
AM_CONDITIONAL(ENABLE_GIO_UNIX, test x$enable_gio_unix = xyes)

PKG_CHECK_MODULES(GIO, [gio-2.0 >= $gio_min_version $gio_unix])
GIO_VALAFLAGS="--pkg gio-2.0 $gio_unix_vala"
AC_SUBST(gio_unix)
AC_SUBST(GIO_VALAFLAGS)

AC_ARG_ENABLE(gdk-pixbuf, AS_HELP_STRING([--disable-gdk-pixbuf], [Disable icon support through gdk-pixbuf]), enable_gdk_pixbuf=$enableval, enable_gdk_pixbuf=yes)
if test x$enable_gdk_pixbuf = xyes; then
    PKG_CHECK_MODULES(GDK_PIXBUF, [gdk-pixbuf-2.0])
    AC_DEFINE([ENABLE_GDK_PIXBUF], [1], [Enable gdk-pixbuf support])
fi


#
# Server and plugins
#

# Server
PKG_CHECK_MODULES(GMODULE, [gmodule-2.0 >= $gmodule_min_version])

AC_ARG_ENABLE(systemd, AS_HELP_STRING([--enable-systemd], [Enable systemd activation support]), enable_systemd=$enableval, enable_systemd=no)
if test x$enable_systemd = xyes; then
    if test x$enable_gio_unix = xno; then
        AC_MSG_ERROR([*** It won’t harm to enable UNIX GIO support])
    fi
    PKG_CHECK_MODULES(SYSTEMD, [libsystemd-daemon])
    AC_DEFINE([ENABLE_SYSTEMD], [1], [Enable systemd activation support])
fi

AC_ARG_ENABLE(dbus, AS_HELP_STRING([--disable-dbus], [Disable DBus support (org.freedesktop.Notifications interface)]), enable_dbus=$enableval, enable_dbus=yes)
AM_CONDITIONAL(ENABLE_DBUS, test x$enable_dbus = xyes)

AC_ARG_ENABLE(eventdctl, AS_HELP_STRING([--disable-eventdctl], [Disable eventdctl]), enable_eventdctl=$enableval, enable_eventdctl=yes)
if test x$enable_eventdctl = xyes; then
    if test x$enable_gio_unix = xno; then
        AC_MSG_ERROR([*** eventdctl requested but not UNIX GIO support])
    fi
fi
AM_CONDITIONAL(ENABLE_EVENTDCTL, test x$enable_eventdctl = xyes)

# sound plugin
AC_ARG_ENABLE(pulseaudio, AS_HELP_STRING([--disable-pulseaudio], [Disable PulseAudio support]), enable_pulseaudio=$enableval, enable_pulseaudio=yes)
if test x$enable_pulseaudio = xyes; then
    PKG_CHECK_MODULES(PULSEAUDIO, [libpulse >= $libpulse_min_version libpulse-mainloop-glib])
fi
AM_CONDITIONAL(ENABLE_PULSEAUDIO, test x$enable_pulseaudio = xyes)

# sndfile plugin
AC_ARG_ENABLE(sndfile, AS_HELP_STRING([--disable-sndfile], [Disable libsndfile support]), enable_sndfile=$enableval, enable_sndfile=yes)
if test x$enable_sndfile = xyes; then
    PKG_CHECK_MODULES(SNDFILE, [sndfile])
fi
AM_CONDITIONAL(ENABLE_SNDFILE, test x$enable_sndfile = xyes)

# espeak plugin
AC_ARG_ENABLE(espeak, AS_HELP_STRING([--enable-espeak], [Enable eSpeak support]), enable_espeak=$enableval, enable_espeak=no)
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

# notification plugin
AC_ARG_ENABLE(libnotify, AS_HELP_STRING([--enable-libnotify], [Enable libnotify support (client side)]), enable_libnotify=$enableval, enable_libnotify=no)
if test x$enable_libnotify = xyes; then
    if test x$enable_gdk_pixbuf = xno; then
        AC_MSG_ERROR([*** libnotify support requested but not gdk-pixbuf support])
    fi
    PKG_CHECK_MODULES(NOTIFY, [libnotify >= $libnotify_min_version])
fi
AM_CONDITIONAL(ENABLE_NOTIFY, [test x$enable_libnotify = xyes])



#
# notification-daemon
#

AC_ARG_ENABLE(graphical-backend, AS_HELP_STRING([--disable-graphical-backend@<:@=BACKEND@:>@], [Disable notification graphical backend @<:@default=xcb@:>@]), enable_graphical_backend=$enableval, enable_graphical_backend=xcb)
AC_ARG_ENABLE(framebuffer-bakend, AS_HELP_STRING([--disable-framebuffer-backend@<:@=BACKEND@:>@], [Disable notificaton framebuffer backend @<:@default=linux@:>@]), enable_framebuffer_backend=$enableval, enable_framebuffer_backend=linux)
if test x$enable_graphical_backend != xno -o x$enable_framebuffer_backend != xno; then
    if test x$enable_gdk_pixbuf = xno; then
        AC_MSG_ERROR([*** notification-daemon requested but not gdk-pixbuf support])
    fi
    case "$enable_graphical_backend" in
        xcb|yes)
            PKG_CHECK_MODULES(XCB, [cairo-xcb xcb xcb-shape])
            AC_DEFINE([ENABLE_XCB], [1], [Enable XCB support])
        ;;
        no) AC_DEFINE([DISABLE_GRAPHICAL_BACKENDS], [1], [No graphical support]) ;;
        *) AC_MSG_ERROR([*** Unknow graphical backend for notification]) ;;
    esac
    case "$enable_framebuffer_backend" in
        linux|yes)
            AC_CHECK_HEADER([linux/fb.h])
            AC_DEFINE([FRAMEBUFFER_TARGET_PREFIX], ["/dev/tty"], [Linux framebuffer target prefix])
            AC_DEFINE([ENABLE_LINUX_FB], [1], [Enable Linux framebuffer support])
        ;;
        no) AC_DEFINE([DISABLE_FRAMEBUFFER_BACKENDS], [1], [No framebuffer support]) ;;
        *) AC_MSG_ERROR([*** Unknow framebuffer backend for notification]) ;;
    esac
    PKG_CHECK_MODULES(CAIRO, [cairo])
    PKG_CHECK_MODULES(PANGO, [pango pangocairo])
fi
AM_CONDITIONAL(ENABLE_NOTIFICATION_DAEMON, [test x$enable_graphical_backend != xno -o x$enable_framebuffer_backend != xno])

#
# Client
#
AC_ARG_ENABLE(client, AS_HELP_STRING([--disable-client], [Disable client (libeventc and eventc)]), enable_client=$enableval, enable_client=yes)
if test x$enable_client = xyes; then
    PKG_CHECK_MODULES(GTHREAD, [gthread-2.0])
    GTHREAD_VALAFLAGS="--thread"
    AC_SUBST(GTHREAD_VALAFLAGS)

    AM_PROG_VALAC([$vala_min_version])
fi
AM_CONDITIONAL(ENABLE_CLIENT, [test x$enable_client = xyes])


# Debug flags
AC_ARG_ENABLE(debug, AS_HELP_STRING([--enable-debug], [Enable debug]), enable_debug=$enableval, enable_debug=no)
if test x$enable_debug = xyes; then
    AC_DEFINE([DEBUG], [1], [Activate debug mode])
    AM_CFLAGS="${AM_CFLAGS} -O0 -ggdb3"
    AM_VALAFLAGS="${AM_VALAFLAGS} --debug -D DEBUG"
fi
AC_SUBST(AM_CFLAGS)
AC_SUBST(AM_LDFLAGS)
AC_SUBST(AM_VALA_CFLAGS)
AC_SUBST(AM_VALAFLAGS)

AM_SUBST_NOTMAKE(LIBS)

AC_CONFIG_FILES([
    data/libeventd-event.pc
    data/libeventc.pc
    Makefile
    vapi/libeventd-event.deps
    vapi/libeventc.deps
])
AC_OUTPUT
