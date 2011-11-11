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
# Helper libs
#

# An helper for config
pkglib_LTLIBRARIES += \
	libeventd-config-helper.la

pkginclude_HEADERS += \
	include/config-helper.h

libeventd_config_helper_la_SOURCES = \
	src/helper-libs/config-helper/config-helper.c

libeventd_config_helper_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GLIB_CFLAGS)

libeventd_config_helper_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version \
	-export-symbols $(top_srcdir)/src/helper-libs/config-helper/config-helper.sym

libeventd_config_helper_la_LIBADD = \
	$(GLIB_LIBS)

# An helper for plugin
pkglib_LTLIBRARIES += \
	libeventd-plugin-helper.la

pkginclude_HEADERS += \
	include/plugin-helper.h

libeventd_plugin_helper_la_SOURCES = \
	src/helper-libs/plugin-helper/plugin-helper.c

libeventd_plugin_helper_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GLIB_CFLAGS)

libeventd_plugin_helper_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version \
	-export-symbols $(top_srcdir)/src/helper-libs/plugin-helper/plugin-helper.sym

libeventd_plugin_helper_la_LIBADD = \
	$(GLIB_LIBS)

# An helper for plugins
pkglib_LTLIBRARIES += \
	libeventd-plugins-helper.la

pkginclude_HEADERS += \
	include/plugins-helper.h \
	include/eventd-plugin.h

libeventd_plugins_helper_la_SOURCES = \
	src/helper-libs/plugins-helper/plugins-helper.c

libeventd_plugins_helper_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D LIBDIR=\"$(libdir)\" \
	-D DATADIR=\"$(datadir)\" \
	$(GMODULE_CFLAGS) \
	$(GLIB_CFLAGS)

libeventd_plugins_helper_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version \
	-export-symbols $(top_srcdir)/src/helper-libs/plugins-helper/plugins-helper.sym

libeventd_plugins_helper_la_LIBADD = \
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
	libeventd-plugins-helper.la \
	$(SYSTEMD_LIBS) \
	$(GIO_LIBS) \
	$(GLIB_LIBS)

# Sound plugin
plugins_LTLIBRARIES += \
	sound.la

sound_la_SOURCES = \
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
	libeventd-plugins-helper.la \
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
	libeventd-config-helper.la \
	libeventd-plugin-helper.la \
	$(SNDFILE_LIBS) \
	$(PULSEAUDIO_LIBS) \
	$(GLIB_LIBS)
endif

# espeak plugin
if ENABLE_ESPEAK
soundplugins_LTLIBRARIES += \
	espeak.la

espeak_la_SOURCES = \
	src/plugins/sound/espeak/espeak.c

espeak_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(ESPEAK_CFLAGS) \
	$(PULSEAUDIO_CFLAGS) \
	$(GLIB_CFLAGS)

espeak_la_LDFLAGS = \
	$(sound_la_LDFLAGS)

espeak_la_LIBADD = \
	libeventd-config-helper.la \
	libeventd-plugin-helper.la \
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
	libeventd-config-helper.la \
	libeventd-plugin-helper.la \
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

# eventdctl
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
