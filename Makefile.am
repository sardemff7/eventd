ACLOCAL_AMFLAGS = -I m4

pluginsdir = $(pkglibdir)/plugins
pulseaudiopluginsdir = $(pluginsdir)/pulseaudio

bin_PROGRAMS =
lib_LTLIBRARIES =
pkglib_LTLIBRARIES =
plugins_LTLIBRARIES =
pulseaudioplugins_LTLIBRARIES =
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

# An helper for config
pkglib_LTLIBRARIES += \
	libeventd-plugin-helper.la

pkginclude_HEADERS += \
	include/plugin-helper.h

libeventd_plugin_helper_la_SOURCES = \
	src/eventd/plugin-helper/plugin-helper.c

libeventd_plugin_helper_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GLIB_CFLAGS)

libeventd_plugin_helper_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version \
	-export-symbols $(top_srcdir)/src/eventd/plugin-helper/plugin-helper.sym

libeventd_plugin_helper_la_LIBADD = \
	$(GLIB_LIBS)

# An helper for plugins
pkglib_LTLIBRARIES += \
	libeventd-plugins-helper.la

pkginclude_HEADERS += \
	include/plugins-helper.h \
	include/eventd-plugin.h

libeventd_plugins_helper_la_SOURCES = \
	src/eventd/plugins-helper/plugins-helper.c

libeventd_plugins_helper_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D LIBDIR=\"$(libdir)\" \
	-D DATADIR=\"$(datadir)\" \
	$(GMODULE_CFLAGS) \
	$(GLIB_CFLAGS)

libeventd_plugins_helper_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version \
	-export-symbols $(top_srcdir)/src/eventd/plugins-helper/plugins-helper.sym

libeventd_plugins_helper_la_LIBADD = \
	$(GMODULE_LIBS) \
	$(GLIB_LIBS)

# Server
bin_PROGRAMS += \
	eventd

eventd_SOURCES = \
	src/eventd/events.h \
	src/eventd/events.c \
	src/eventd/plugins.h \
	src/eventd/plugins.c \
	src/eventd/service.h \
	src/eventd/service.c \
	src/eventd/eventd.h \
	src/eventd/eventd.c

eventd_CFLAGS = \
	$(AM_CFLAGS) \
	-D SYSCONFDIR=\"$(sysconfdir)\" \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS)

eventd_LDADD = \
	libeventd-plugins-helper.la \
	$(GIO_LIBS) \
	$(GLIB_LIBS)


if ENABLE_SYSTEMD
eventd_CFLAGS += \
	$(SYSTEMD_CFLAGS)

eventd_LDADD += \
	$(SYSTEMD_LIBS)
endif

if ENABLE_PULSEAUDIO
plugins_LTLIBRARIES += \
	pulseaudio.la

pulseaudio_la_SOURCES = \
	src/plugins/pulseaudio/pulseaudio.c

pulseaudio_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(SNDFILE_CFLAGS) \
	$(PULSEAUDIO_CFLAGS) \
	$(GLIB_CFLAGS)

pulseaudio_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module -export-symbols-regex eventd_plugin_get_info

pulseaudio_la_LIBADD = \
	libeventd-plugins-helper.la \
	$(PULSEAUDIO_LIBS) \
	$(GLIB_LIBS)

if ENABLE_SNDFILE
pulseaudioplugins_LTLIBRARIES += \
	sndfile.la

sndfile_la_SOURCES = \
	src/plugins/pulseaudio/sndfile/sndfile.c

sndfile_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(SNDFILE_CFLAGS) \
	$(PULSEAUDIO_CFLAGS) \
	$(GLIB_CFLAGS)

sndfile_la_LDFLAGS = \
	$(pulseaudio_la_LDFLAGS)

sndfile_la_LIBADD = \
	libeventd-plugin-helper.la \
	$(SNDFILE_LIBS) \
	$(PULSEAUDIO_LIBS) \
	$(GLIB_LIBS)
endif

if ENABLE_ESPEAK
pulseaudioplugins_LTLIBRARIES += \
	espeak.la

espeak_la_SOURCES = \
	src/plugins/pulseaudio/espeak/espeak.c

espeak_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(ESPEAK_CFLAGS) \
	$(PULSEAUDIO_CFLAGS) \
	$(GLIB_CFLAGS)

espeak_la_LDFLAGS = \
	$(pulseaudio_la_LDFLAGS)

espeak_la_LIBADD = \
	libeventd-plugin-helper.la \
	$(ESPEAK_LIBS) \
	$(PULSEAUDIO_LIBS) \
	$(GLIB_LIBS)
endif

endif

if ENABLE_NOTIFY
plugins_LTLIBRARIES += \
	notify.la

notify_la_SOURCES = \
	src/plugins/notify/notify.c

notify_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(NOTIFY_CFLAGS) \
	$(GLIB_CFLAGS)

notify_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module -export-symbols-regex eventd_plugin_get_info

notify_la_LIBADD = \
	libeventd-plugin-helper.la \
	$(NOTIFY_LIBS) \
	$(GLIB_LIBS)
endif

if ENABLE_DIALOGS
plugins_LTLIBRARIES += \
	dialogs.la

dialogs_la_SOURCES = \
	src/plugins/dialogs/dialogs.c

dialogs_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS)

dialogs_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module -export-symbols-regex eventd_plugin_get_info

dialogs_la_LIBADD = \
	libeventd-plugin-helper.la \
	$(GTK_LIBS) \
	$(GIO_LIBS) \
	$(GLIB_LIBS)
endif


# Client

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
	--pkg eventd-common \
	$(GIO_VALAFLAGS) \
	$(GOBJECT_VALAFLAGS) \
	$(GLIB_VALAFLAGS)

libeventc_la_CFLAGS = \
	$(AM_CFLAGS) \
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


# Hooks
install-data-hook: la-files-removal

# *.la files cleanup
la-files-removal:
	cd $(DESTDIR)/$(libdir) && \
		rm $(lib_LTLIBRARIES)
	cd $(DESTDIR)/$(pkglibdir) && \
		rm $(pkglib_LTLIBRARIES)
	cd $(DESTDIR)/$(pluginsdir) && \
		rm $(plugins_LTLIBRARIES)
	cd $(DESTDIR)/$(pulseaudiopluginsdir) && \
		rm $(pulseaudioplugins_LTLIBRARIES)
