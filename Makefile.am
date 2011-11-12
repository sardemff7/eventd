ACLOCAL_AMFLAGS = -I m4

pluginsdir = $(pkglibdir)/plugins
soundpluginsdir = $(pluginsdir)/sound

bin_PROGRAMS =
lib_LTLIBRARIES =
pkglib_LTLIBRARIES =
plugins_LTLIBRARIES =
soundplugins_LTLIBRARIES =
pkginclude_HEADERS =

CLEANFILES =

AM_CFLAGS += \
	--include config.h \
	-I $(top_srcdir)/include/ \
	-I $(top_builddir)/include/

AM_VALAFLAGS += \
	--vapidir $(top_srcdir)/vapi/ \
	--vapidir $(top_builddir)/vapi/ \
	--pkg config


#
# Helper lib
#
pkglib_LTLIBRARIES += \
	libeventd.la

pkginclude_HEADERS += \
	include/libeventd-regex.h \
	include/libeventd-config.h \
	include/libeventd-plugins.h

libeventd_la_SOURCES = \
	src/libeventd/regex.c \
	src/libeventd/config.c \
	src/libeventd/plugins.c

libeventd_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D LIBDIR=\"$(libdir)\" \
	-D DATADIR=\"$(datadir)\" \
	$(GMODULE_CFLAGS) \
	$(GLIB_CFLAGS)

libeventd_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version \
	-export-symbols $(top_srcdir)/src/libeventd/libeventd.sym

libeventd_la_LIBADD = \
	$(GMODULE_LIBS) \
	$(GLIB_LIBS)


#
# Server and plugins
#

# Server
bin_PROGRAMS += \
	eventd

eventd_SOURCES = \
	src/eventd/config.h \
	src/eventd/config.c \
	src/eventd/plugins.h \
	src/eventd/plugins.c \
	src/eventd/service.h \
	src/eventd/service.c \
	src/eventd/eventd.h \
	src/eventd/eventd.c

eventd_CFLAGS = \
	$(AM_CFLAGS) \
	-D SYSCONFDIR=\"$(sysconfdir)\" \
	-D DATADIR=\"$(datadir)\" \
	$(SYSTEMD_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS)

eventd_LDADD = \
	libeventd.la \
	$(SYSTEMD_LIBS) \
	$(GIO_LIBS) \
	$(GLIB_LIBS)

# eventdctl
if ENABLE_EVENTDCTL
bin_PROGRAMS += \
	eventdctl

eventdctl_SOURCES = \
	src/eventdctl/eventdctl.c

eventdctl_CFLAGS = \
	$(AM_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS)

eventdctl_LDADD = \
	$(GIO_LIBS) \
	$(GLIB_LIBS)
endif

# Sound plugin
plugins_LTLIBRARIES += \
	sound.la

sound_la_SOURCES = \
	src/plugins/sound/pulseaudio-internal.h \
	src/plugins/sound/pulseaudio.h \
	src/plugins/sound/sound.c

sound_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(SNDFILE_CFLAGS) \
	$(PULSEAUDIO_CFLAGS) \
	$(GLIB_CFLAGS)

sound_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module -export-symbols-regex eventd_plugin_get_info

sound_la_LIBADD = \
	libeventd.la \
	$(PULSEAUDIO_LIBS) \
	$(GLIB_LIBS)

if ENABLE_PULSEAUDIO
sound_la_SOURCES += \
	src/plugins/sound/pulseaudio.c
else
sound_la_SOURCES += \
	src/plugins/sound/pulseaudio-dummy.c
endif

# sndfile plugin
if ENABLE_SNDFILE
soundplugins_LTLIBRARIES += \
	sndfile.la

sndfile_la_SOURCES = \
	src/plugins/sound/sndfile/sndfile.c

sndfile_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(SNDFILE_CFLAGS) \
	$(PULSEAUDIO_CFLAGS) \
	$(GLIB_CFLAGS)

sndfile_la_LDFLAGS = \
	$(sound_la_LDFLAGS)

sndfile_la_LIBADD = \
	libeventd.la \
	$(SNDFILE_LIBS) \
	$(PULSEAUDIO_LIBS) \
	$(GLIB_LIBS)
endif

# espeak plugin
if ENABLE_ESPEAK
soundplugins_LTLIBRARIES += \
	espeak.la

espeak_la_SOURCES = \
	src/plugins/sound/espeak/pulseaudio.h \
	src/plugins/sound/espeak/espeak.h \
	src/plugins/sound/espeak/espeak.c

espeak_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(ESPEAK_CFLAGS) \
	$(PULSEAUDIO_CFLAGS) \
	$(GLIB_CFLAGS)

espeak_la_LDFLAGS = \
	$(sound_la_LDFLAGS)

espeak_la_LIBADD = \
	libeventd.la \
	$(ESPEAK_LIBS) \
	$(PULSEAUDIO_LIBS) \
	$(GLIB_LIBS)

if ENABLE_PULSEAUDIO
espeak_la_SOURCES += \
	src/plugins/sound/espeak/pulseaudio.c
else
espeak_la_SOURCES += \
	src/plugins/sound/espeak/pulseaudio-dummy.c
endif
endif

# notification plugin
plugins_LTLIBRARIES += \
	notification.la

notification_la_SOURCES = \
	src/plugins/notification/libnotify-compat.h \
	src/plugins/notification/notify.h \
	src/plugins/notification/icon.h \
	src/plugins/notification/notification.h \
	src/plugins/notification/notification.c

notification_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GDK_PIXBUF_CFLAGS) \
	$(NOTIFY_CFLAGS) \
	$(GLIB_CFLAGS)

notification_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module -export-symbols-regex eventd_plugin_get_info

notification_la_LIBADD = \
	libeventd.la \
	$(GDK_PIXBUF_LIBS) \
	$(NOTIFY_LIBS) \
	$(GLIB_LIBS)

if ENABLE_NOTIFY
notification_la_SOURCES += \
	src/plugins/notification/notify.c
else
notification_la_SOURCES += \
	src/plugins/notification/notify-dummy.c
endif

if ENABLE_GDK_PIXBUF
notification_la_SOURCES += \
	src/plugins/notification/icon.c
else
notification_la_SOURCES += \
	src/plugins/notification/icon-dummy.c
endif


#
# Client
#

# Client library
if ENABLE_CLIENT
lib_LTLIBRARIES += \
	libeventc.la

LIBEVENTC_CURRENT=0
LIBEVENTC_REVISION=0
LIBEVENTC_AGE=0

libeventc_la_SOURCES = \
	src/libeventc/libeventc.vala

libeventc_la_VALAFLAGS = \
	$(AM_VALAFLAGS) \
	--library $(top_builddir)/vapi/libeventc \
	-H $(top_builddir)/include/libeventc.h \
	$(GIO_VALAFLAGS) \
	$(GOBJECT_VALAFLAGS) \
	$(GLIB_VALAFLAGS)

libeventc_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(AM_VALA_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

libeventc_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-shared \
	-version-info $(LIBEVENTC_CURRENT):$(LIBEVENTC_REVISION):$(LIBEVENTC_AGE) \
	-Wl,--version-script=$(top_srcdir)/src/libeventc/libeventc.sym

libeventc_la_LIBADD = \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)

pkginclude_HEADERS += \
	$(top_builddir)/include/libeventc.h

vapidir = $(datadir)/vala/vapi
dist_vapi_DATA = $(top_builddir)/vapi/libeventc.vapi

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(top_builddir)/data/libeventc.pc

CLEANFILES += \
	$(top_builddir)/include/libeventc.h \
	$(top_builddir)/vapi/libeventc.vapi \
	$(top_builddir)/vapi/libeventc.deps \
	$(libeventc_la_SOURCES:.vala=.c)

$(top_builddir)/vapi/libeventc.vapi: libeventc.la

# Basic CLI client
bin_PROGRAMS += \
	eventc

eventc_SOURCES = \
	src/eventc/eventc.vala

eventc_VALAFLAGS = \
	$(AM_VALAFLAGS) \
	--pkg libeventc \
	$(GIO_VALAFLAGS) \
	$(GOBJECT_VALAFLAGS) \
	$(GLIB_VALAFLAGS)

eventc_CFLAGS = \
	$(AM_CFLAGS) \
	$(AM_VALA_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

eventc_LDADD = \
	libeventc.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)

CLEANFILES += \
	$(eventc_SOURCES:.vala=.c)

eventc_vala.stamp: $(top_builddir)/vapi/libeventc.vapi
endif


#
# Hooks
#

install-data-hook: la-files-removal

# *.la files cleanup
la-files-removal:
	cd $(DESTDIR)/$(libdir) && \
		rm $(lib_LTLIBRARIES)
	cd $(DESTDIR)/$(pkglibdir) && \
		rm $(pkglib_LTLIBRARIES)
	cd $(DESTDIR)/$(pluginsdir) && \
		rm $(plugins_LTLIBRARIES)
	cd $(DESTDIR)/$(soundpluginsdir) && \
		rm $(soundplugins_LTLIBRARIES)
