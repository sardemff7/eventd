ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS =
lib_LTLIBRARIES =
pkginclude_HEADERS =

CLEANFILES =

AM_CFLAGS += \
	-I $(top_srcdir)/include/ \
	-I $(top_builddir)/include/

AM_VALAFLAGS += \
	--vapidir $(top_srcdir)/vapi/ \
	--vapidir $(top_builddir)/vapi/ \
	--pkg config


# Server
bin_PROGRAMS += \
	eventd

eventd_SOURCES = \
	src/eventd/events.h \
	src/eventd/events.c \
	src/eventd/service.h \
	src/eventd/service.c \
	src/eventd/eventd.h \
	src/eventd/eventd.c

eventd_CFLAGS = \
	$(AM_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS)

eventd_LDADD = \
	$(GTK_LIBS) \
	$(GIO_LIBS) \
	$(GLIB_LIBS)


if ENABLE_SYSTEMD
eventd_CFLAGS += \
	$(SYSTEMD_CFLAGS)

eventd_LDADD += \
	$(SYSTEMD_LIBS)
endif

if ENABLE_SOUND
eventd_SOURCES += \
	src/eventd/pulse.h \
	src/eventd/pulse.c

eventd_CFLAGS += \
	$(SOUND_CFLAGS)

eventd_LDADD += \
	$(SOUND_LIBS)
endif

if ENABLE_NOTIFY
eventd_SOURCES += \
	src/eventd/notify.h \
	src/eventd/notify.c

eventd_CFLAGS += \
	$(NOTIFY_CFLAGS)

eventd_LDADD += \
	$(NOTIFY_LIBS)
endif

if ENABLE_DIALOGS
eventd_SOURCES += \
	src/eventd/dialogs.h \
	src/eventd/dialogs.c

eventd_CFLAGS += \
	$(GTK_CFLAGS)

eventd_LDADD += \
	$(GTK_LIBS)
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
install-exec-hook: la-files-removal

# *.la files cleanup
la-files-removal:
	cd $(DESTDIR)/$(libdir) && \
		rm $(lib_LTLIBRARIES)
