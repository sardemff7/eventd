ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS =
lib_LTLIBRARIES =
pkginclude_HEADERS =

CLEANFILES =

AM_CFLAGS = \
	-I $(top_srcdir)/include/ \
	-I $(top_builddir)/include/ \
	$(GLOBAL_CFLAGS)

AM_LDFLAGS = \
	$(GLOBAL_LDFLAGS)

AM_VALAFLAGS = \
	--vapidir $(top_srcdir)/vapi/ \
	--vapidir $(top_builddir)/vapi/ \
	--pkg config \
	$(GLOBAL_VALAFLAGS)



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
	$(GLIB_VALAFLAGS)

if ENABLE_GIO_UNIX
libeventc_la_VALAFLAGS += \
	-D ENABLE_GIO_UNIX
endif

libeventc_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS)

libeventc_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-shared \
	-version-info $(LIBEVENTC_CURRENT):$(LIBEVENTC_REVISION):$(LIBEVENTC_AGE) \
	-Wl,--version-script=$(top_srcdir)/src/libeventc/libeventc.sym

libeventc_la_LIBADD = \
	$(GIO_LIBS) \
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
	$(libeventc_la_SOURCES:.vala=.c)

$(top_builddir)/vapi/libeventc.vapi: libeventc.la


bin_PROGRAMS += \
	eventc

eventc_SOURCES = \
	$(top_builddir)/vapi/libeventc.vapi \
	src/eventc/eventc.vala

eventc_VALAFLAGS = \
	$(AM_VALAFLAGS) \
	$(GLIB_VALAFLAGS)

if ENABLE_GIO_UNIX
eventc_VALAFLAGS += \
	-D ENABLE_GIO_UNIX
endif

eventc_CFLAGS = \
	$(AM_CFLAGS) \
	$(GLIB_CFLAGS)

eventc_LDADD = \
	libeventc.la \
	$(GLIB_LIBS)

CLEANFILES += \
	$(eventc_SOURCES:.vala=.c)


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
	-DLOCALEDIR=\"$(localedir)\" \
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

install-exec-hook: la-files-removal

la-files-removal:
	cd $(DESTDIR)/$(libdir) && \
		rm $(lib_LTLIBRARIES)
