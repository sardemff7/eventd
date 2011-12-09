ACLOCAL_AMFLAGS = -I m4

pkgconfigdir = $(libdir)/pkgconfig
vapidir = $(datadir)/vala/vapi
pluginsdir = $(pkglibdir)/plugins
soundpluginsdir = $(pluginsdir)/sound

bin_PROGRAMS =
lib_LTLIBRARIES =
pkglib_LTLIBRARIES =
plugins_LTLIBRARIES =
soundplugins_LTLIBRARIES =
pkginclude_HEADERS =
pkgconfig_DATA =
dist_vapi_DATA =
pkgdata_DATA =

CLEANFILES =

AM_CFLAGS += \
	-D LOCALEDIR=\"$(localedir)\" \
	--include config.h \
	-I $(top_srcdir)/include/ \
	-I $(top_builddir)/include/

AM_VALAFLAGS += \
	--vapidir $(top_srcdir)/vapi/ \
	--vapidir $(top_builddir)/vapi/ \
	--pkg config



#
# Event lib
#
LIBEVENTD_EVENT_CURRENT=0
LIBEVENTD_EVENT_REVISION=0
LIBEVENTD_EVENT_AGE=0

lib_LTLIBRARIES += \
	libeventd-event.la

pkginclude_HEADERS += \
	include/libeventd-event-types.h \
	include/libeventd-event.h

libeventd_event_la_SOURCES = \
	src/libeventd-event/event.c

libeventd_event_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"libeventd-event\" \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

libeventd_event_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-version-info $(LIBEVENTD_EVENT_CURRENT):$(LIBEVENTD_EVENT_REVISION):$(LIBEVENTD_EVENT_AGE) \
	-Wl,--version-script=$(top_srcdir)/src/libeventd-event/libeventd-event.sym

libeventd_event_la_LIBADD = \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)

pkgconfig_DATA += \
	$(top_builddir)/data/libeventd-event.pc

dist_vapi_DATA += \
	$(top_builddir)/vapi/libeventd-event.deps \
	vapi/libeventd-event.vapi


#
# Internal helper lib
#
pkglib_LTLIBRARIES += \
	libeventd.la

pkginclude_HEADERS += \
	include/libeventd-types.h \
	include/libeventd-regex.h \
	include/libeventd-config.h \
	include/libeventd-plugins.h \
	include/libeventd-client.h

libeventd_la_SOURCES = \
	src/libeventd/regex.c \
	src/libeventd/config.c \
	src/libeventd/plugins.c \
	src/libeventd/client.c

libeventd_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"libeventd\" \
	-D LIBDIR=\"$(libdir)\" \
	-D DATADIR=\"$(datadir)\" \
	$(GMODULE_CFLAGS) \
	$(GLIB_CFLAGS)

libeventd_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version \
	-export-symbols $(top_srcdir)/src/libeventd/libeventd.sym

libeventd_la_LIBADD = \
	libeventd-event.la \
	$(GMODULE_LIBS) \
	$(GLIB_LIBS)


#
# Server and plugins
#

# Server
bin_PROGRAMS += \
	eventd

eventd_SOURCES = \
	src/eventd/types.h \
	src/eventd/config.h \
	src/eventd/config.c \
	src/eventd/plugins.h \
	src/eventd/plugins.c \
	src/eventd/queue.h \
	src/eventd/queue.c \
	src/eventd/control.h \
	src/eventd/control.c \
	src/eventd/sockets.h \
	src/eventd/sockets.c \
	src/eventd/dbus.h \
	src/eventd/service.h \
	src/eventd/service.c \
	src/eventd/eventd.h \
	src/eventd/eventd.c

eventd_CFLAGS = \
	$(AM_CFLAGS) \
	-D SYSCONFDIR=\"$(sysconfdir)\" \
	-D DATADIR=\"$(datadir)\" \
	$(SYSTEMD_CFLAGS) \
	$(GTHREAD_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS)

eventd_LDADD = \
	libeventd-event.la \
	libeventd.la \
	$(SYSTEMD_LIBS) \
	$(GTHREAD_LIBS) \
	$(GIO_LIBS) \
	$(GLIB_LIBS)

if ENABLE_DBUS
eventd_SOURCES += \
	src/eventd/dbus.c
else
eventd_SOURCES += \
	src/eventd/dbus-dummy.c
endif

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
	libeventd-event.la \
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
	src/plugins/sound/sndfile/pulseaudio.h \
	src/plugins/sound/sndfile/sndfile.c

sndfile_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(SNDFILE_CFLAGS) \
	$(PULSEAUDIO_CFLAGS) \
	$(GLIB_CFLAGS)

sndfile_la_LDFLAGS = \
	$(sound_la_LDFLAGS)

sndfile_la_LIBADD = \
	libeventd-event.la \
	libeventd.la \
	$(SNDFILE_LIBS) \
	$(PULSEAUDIO_LIBS) \
	$(GLIB_LIBS)

if ENABLE_PULSEAUDIO
sndfile_la_SOURCES += \
	src/plugins/sound/sndfile/pulseaudio.c
else
sndfile_la_SOURCES += \
	src/plugins/sound/sndfile/pulseaudio-dummy.c
endif
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
	libeventd-event.la \
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
	src/plugins/notification/icon.c \
	src/plugins/notification/daemon.h \
	src/plugins/notification/notification.h \
	src/plugins/notification/notification.c

notification_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D SYSCONFDIR=\"$(sysconfdir)\" \
	-D LIBDIR=\"$(libdir)\" \
	-D DATADIR=\"$(datadir)\" \
	$(NOTIFY_CFLAGS) \
	$(XCB_CFLAGS) \
	$(CAIRO_CFLAGS) \
	$(PANGO_CFLAGS) \
	$(GDK_PIXBUF_CFLAGS) \
	$(GLIB_CFLAGS)

notification_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module -export-symbols-regex eventd_plugin_get_info

notification_la_LIBADD = \
	libeventd-event.la \
	libeventd.la \
	$(NOTIFY_LIBS) \
	$(XCB_LIBS) \
	$(CAIRO_LIBS) \
	$(PANGO_LIBS) \
	$(GDK_PIXBUF_LIBS) \
	$(GLIB_LIBS)

if ENABLE_NOTIFICATION_DAEMON
notification_la_SOURCES += \
	src/plugins/notification/daemon/backends/fb.h \
	src/plugins/notification/daemon/backends/linux.c \
	src/plugins/notification/daemon/backends/graphical.h \
	src/plugins/notification/daemon/backends/xcb.c \
	src/plugins/notification/daemon/backends/dummy.c \
	src/plugins/notification/daemon/style.h \
	src/plugins/notification/daemon/style.c \
	src/plugins/notification/daemon/bubble.h \
	src/plugins/notification/daemon/bubble.c \
	src/plugins/notification/daemon/types.h \
	src/plugins/notification/daemon/daemon.c
else
notification_la_SOURCES += \
	src/plugins/notification/daemon/dummy.c
endif

if ENABLE_NOTIFY
notification_la_SOURCES += \
	src/plugins/notification/notify.c
else
notification_la_SOURCES += \
	src/plugins/notification/notify-dummy.c
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
	--pkg libeventd-event \
	--library $(top_builddir)/vapi/libeventc \
	-H $(top_builddir)/include/libeventc.h \
	$(GIO_VALAFLAGS) \
	$(GOBJECT_VALAFLAGS) \
	$(GTHREAD_VALAFLAGS) \
	$(GLIB_VALAFLAGS)

libeventc_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"libeventc\" \
	$(AM_VALA_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GTHREAD_CFLAGS) \
	$(GLIB_CFLAGS)

libeventc_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-shared \
	-version-info $(LIBEVENTC_CURRENT):$(LIBEVENTC_REVISION):$(LIBEVENTC_AGE) \
	-Wl,--version-script=$(top_srcdir)/src/libeventc/libeventc.sym

libeventc_la_LIBADD = \
	libeventd-event.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GTHREAD_LIBS) \
	$(GLIB_LIBS)

pkginclude_HEADERS += \
	$(top_builddir)/include/libeventc.h

pkgconfig_DATA += \
	$(top_builddir)/data/libeventc.pc

dist_vapi_DATA += \
	$(top_builddir)/vapi/libeventc.deps \
	$(top_builddir)/vapi/libeventc.vapi

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
	--pkg libeventd-event \
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
	libeventd-event.la \
	libeventc.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)

CLEANFILES += \
	$(eventc_SOURCES:.vala=.c)

eventc_vala.stamp: $(top_builddir)/vapi/libeventc.vapi
endif


#
# Basic configurations
#

if ENABLE_DBUS
pkgdata_DATA += \
	data/libnotify.conf
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
