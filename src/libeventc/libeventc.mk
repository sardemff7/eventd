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
	--pkg libeventd-event-private \
	--pkg libeventd-evp \
	--library libeventc \
	--vapi vapi/libeventc.vapi \
	--header include/libeventc.h \
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
	-version-info $(LIBEVENTC_CURRENT):$(LIBEVENTC_REVISION):$(LIBEVENTC_AGE)

libeventc_la_LIBADD = \
	libeventd-event.la \
	libeventd-evp.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)

pkginclude_HEADERS += \
	include/libeventc.h

pkgconfig_DATA += \
	data/libeventc.pc

dist_vapi_DATA += \
	vapi/libeventc.deps \
	vapi/libeventc.vapi
