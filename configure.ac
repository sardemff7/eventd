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

AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$PACKAGE_NAME"], [Gettext Package])
AC_DEFINE_UNQUOTED([LOCALEDIR], ["$localedir"], [Locale directory])
IT_PROG_INTLTOOL([0.35.0])

AC_PROG_INSTALL


# Common dependencies
glib_min_version="2.25.0"
gio_min_version="$glib_min_version"

# Server dependencies
libpulse_min_version="0.9.15"
libnotify_min_version="0.6.5"
gtk_min_version="3.0"

# Client dependencies
vala_min_version="0.12"


AM_CFLAGS="-Wall -Wextra -Wno-unused-but-set-variable -Wno-unused-parameter"
AM_LDFLAGS=""
AM_VALAFLAGS=""


# Common dependencies
PKG_CHECK_MODULES(GLIB, [glib-2.0 >= $glib_min_version])
GLIB_VALAFLAGS="--pkg glib-2.0 --pkg glib-local-2.0"
AC_SUBST(GLIB_VALAFLAGS)

gio_unix=
gio_unix_vala=
AC_ARG_ENABLE(gio-unix, AS_HELP_STRING([--disable-gio-unix], [Disable UNIX-specific GIO support]), enable_gio_unix=$enableval, enable_gio_unix=yes)
if test x$enable_gio_unix = xyes; then
  gio_unix="gio-unix-2.0"
  gio_unix_vala="--pkg gio-unix-2.0"
  AM_VALAFLAGS="${AM_VALAFLAGS} -D ENABLE_GIO_UNIX"
  AC_DEFINE([ENABLE_GIO_UNIX], [1], [Enable UNIX-specific GIO support])
fi
AM_CONDITIONAL(ENABLE_GIO_UNIX, test x$enable_gio_unix = xyes)

PKG_CHECK_MODULES(GIO, [gio-2.0 >= $gio_min_version $gio_unix])
GIO_VALAFLAGS="--pkg gio-2.0 $gio_unix_vala"
AC_SUBST(gio_unix)
AC_SUBST(GIO_VALAFLAGS)


# Server dependencies
AC_ARG_ENABLE(systemd, AS_HELP_STRING([--disable-systemd], [Disable systemd support activation support]), enable_systemd=$enableval, enable_systemd=yes)
if test x$enable_systemd = xyes; then
  PKG_CHECK_MODULES(SYSTEMD, [libsystemd-daemon])
  AC_DEFINE([ENABLE_SYSTEMD], [1], [Enable systemd activation support])
fi
AM_CONDITIONAL(ENABLE_SYSTEMD, test x$enable_systemd = xyes)


AC_ARG_ENABLE(sound, AS_HELP_STRING([--disable-sound], [Disable sound support]), enable_sound=$enableval, enable_sound=yes)
if test x$enable_sound = xyes; then
  PKG_CHECK_MODULES(SOUND, [libpulse >= $libpulse_min_version sndfile])
  AC_DEFINE([ENABLE_SOUND], [1], [Enable sound support through PulseAudio and libsndfile])
fi
AM_CONDITIONAL(ENABLE_SOUND, test x$enable_sound = xyes)

AC_ARG_ENABLE(libnotify, AS_HELP_STRING([--disable-libnotify], [Disable libnotify support]), enable_libnotify=$enableval, enable_libnotify=yes)
if test x$enable_libnotify = xyes; then
  PKG_CHECK_MODULES(NOTIFY, [libnotify >= $libnotify_min_version])
  AC_DEFINE([ENABLE_NOTIFY], [1], [Enable libnotify support])
fi
AM_CONDITIONAL(ENABLE_NOTIFY, [test x$enable_libnotify = xyes])

AC_ARG_ENABLE(dialogs, AS_HELP_STRING([--disable-dialogs], [Disable dialog support]), enable_dialogs=$enableval, enable_dialogs=no)
#AC_ARG_ENABLE(gtk, AS_HELP_STRING([--enable-gtk], [Enable GTK+]), enable_gtk=$enableval, enable_gtk=no)
if test x$enable_dialogs = xyes; then
  AC_CHECK_PROG(have_zenity, zenity, yes, no)
  if test x$enable_gtk = xyes; then
    PKG_CHECK_MODULES(GTK, [gtk+-3.0 >= $gtk_min_version])
    AC_DEFINE([HAVE_DIALOGS], [1], [If we have a dialog support])
    AC_DEFINE([ENABLE_GTK], [1], [Enable native GTK+ support])
  elif test x$have_zenity = xyes; then
    AC_DEFINE([HAVE_DIALOGS], [1], [If we have a dialog support])
  else
    AC_MSG_ERROR([*** Dialog support requested but zenity not found])
  fi
elif test x$enable_gtk = xyes; then
  AC_MSG_ERROR([*** GTK+ support requested but not dialog support])
fi
AM_CONDITIONAL(ENABLE_DIALOGS, [test x$enable_dialogs = xyes])


# Client dependencies
AC_ARG_ENABLE(client, AS_HELP_STRING([--disable-client], [Disable client (lib and ]), enable_client=$enableval, enable_client=yes)
if test x$enable_client = xyes; then
    PKG_CHECK_MODULES(GOBJECT, [gobject-2.0])
    GOBJECT_VALAFLAGS="--pkg gobject-2.0"
    AC_SUBST(GOBJECT_VALAFLAGS)

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
AC_SUBST(AM_VALAFLAGS)

AC_CONFIG_FILES([
  data/libeventc.pc
  Makefile
  po/Makefile.in
  vapi/libeventc.deps
])
AC_OUTPUT


echo GIO UNIX support:     $enable_gio_unix

echo systemd support:      $enable_systemd
echo Libnotify support:    $enable_notify
echo Sound support:        $enable_sound
echo Dialogs support:      $enable_dialogs
#echo  Using GTK+:          $enable_gtk
