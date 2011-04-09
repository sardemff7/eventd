AC_PREREQ([2.65])
AC_INIT([Eventd],[0.1],[],[])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src/])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([.aux])

AC_SUBST([PACKAGE_NAME], ["$PACKAGE_NAME"])
AC_SUBST([PACKAGE_VERSION], ["$PACKAGE_VERSION"])

AM_INIT_AUTOMAKE([1.10 dist-xz no-dist-gzip foreign])
AM_MAINTAINER_MODE

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_PROG_CC

GETTEXT_PACKAGE=$PACKAGE_NAME
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Gettext Package])
AC_SUBST(GETTEXT_PACKAGE)
AM_GLIB_GNU_GETTEXT
IT_PROG_INTLTOOL([0.35.0])

AC_SUBST([localedir], ["$localedir"])

AC_PROG_INSTALL

PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.25.0])
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

gio_unix=
AC_ARG_ENABLE(gio-unix, AS_HELP_STRING([--disable-gio-unix], [Disable UNIX-specific GIO support]), enable_gio_unix=$enableval, enable_gio_unix=yes)
if test x$enable_gio_unix = xyes; then
  gio_unix="gio-unix-2.0"
  AC_DEFINE([ENABLE_GIO_UNIX], [1], [Enable UNIX-specific GIO support])
fi

PKG_CHECK_MODULES(GIO, [gio-2.0 >= 2.25.0 $gio_unix])
AC_SUBST(GIO_CFLAGS)
AC_SUBST(GIO_LIBS)


AM_CONDITIONAL(ENABLE_SOUND, false)
AC_ARG_ENABLE(sound, AS_HELP_STRING([--disable-sound], [Disable sound support]), enable_sound=$enableval, enable_sound=yes)
if test x$enable_sound = xyes; then
  PKG_CHECK_MODULES(PULSE, [libpulse >= 0.9.15])
  PKG_CHECK_MODULES(SNDFILE, [sndfile])
  AC_DEFINE([ENABLE_SOUND], [1], [Enable sound support through PulseAudio and libsndfile])
  AM_CONDITIONAL(ENABLE_SOUND, true)
fi
AC_SUBST(PULSE_CFLAGS)
AC_SUBST(PULSE_LIBS)
AC_SUBST(SNDFILE_CFLAGS)
AC_SUBST(SNDFILE_LIBS)

AM_CONDITIONAL(ENABLE_NOTIFY, false)
AC_ARG_ENABLE(notify, AS_HELP_STRING([--disable-notify], [Disable libnotify support]), enable_notify=$enableval, enable_notify=yes)
if test x$enable_notify = xyes; then
  PKG_CHECK_MODULES(NOTIFY, [libnotify >= 0.6.5])
  AC_DEFINE([ENABLE_NOTIFY], [1], [Enable libnotify support])
  AM_CONDITIONAL(ENABLE_NOTIFY, true)
fi
AC_SUBST(NOTIFY_CFLAGS)
AC_SUBST(NOTIFY_LIBS)

AC_ARG_ENABLE(dialogs, AS_HELP_STRING([--disable-dialogs], [Disable dialog support]), enable_dialogs=$enableval, enable_dialogs=yes)
if test x$enable_dialogs = xyes; then
  AC_CHECK_PROG(have_zenity, zenity, yes, no)
  #AC_ARG_ENABLE(gtk, AS_HELP_STRING([--enable-gtk], [Enable GTK+]), enable_gtk=$enableval, enable_gtk=no)
  if test x$enable_gtk = xyes; then
    PKG_CHECK_MODULES(GTK, [gtk+-3.0 >= 3.0])
    AC_DEFINE([HAVE_DIALOGS], [1], [If we have a dialog support])
    AC_DEFINE([ENABLE_GTK], [1], [Enable native GTK+ support])
  elif test x$have_zenity = xyes; then
    AC_DEFINE([HAVE_DIALOGS], [1], [If we have a dialog support])
  fi
fi
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

GLOBAL_CFLAGS="-Wall -Wextra"
GLOBAL_LDFLAGS=""
AC_ARG_ENABLE(debug, AS_HELP_STRING([--enable-debug], [Enable debug]), enable_debug=$enableval, enable_debug=no)
if test x$enable_debug = xyes; then
  AC_DEFINE([DEBUG], [1], [Activate debug mode])
  GLOBAL_CFLAGS+=" -Werror -O0 -ggdb3"
fi
AC_SUBST(GLOBAL_CFLAGS)
AC_SUBST(GLOBAL_LDFLAGS)

AC_CONFIG_FILES([
  Makefile
  po/Makefile.in
])
AC_OUTPUT


echo Libnotify support:    $enable_notify
echo Sound support:        $enable_sound
echo Dialogs support:      $enable_dialogs
echo  Using GTK+:          $enable_gtk
