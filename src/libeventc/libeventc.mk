# Client library
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
	$(GLIB_VALAFLAGS)

libeventc_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"libeventc\" \
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
	libeventd-event.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
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
