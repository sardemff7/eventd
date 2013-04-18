# Client library

LIBEVENTC_CURRENT=0
LIBEVENTC_REVISION=0
LIBEVENTC_AGE=0


AM_CPPFLAGS += \
	-I $(srcdir)/client/libeventc/include


lib_LTLIBRARIES += \
	libeventc.la

pkginclude_HEADERS += \
	client/libeventc/include/libeventc.h

TESTS += \
	libeventc-connection.test

check_PROGRAMS += \
	libeventc-connection.test

pkgconfig_DATA += \
	client/libeventc/pkgconfig/libeventc.pc

dist_vapi_DATA += \
	client/libeventc/vapi/libeventc.vapi

vapi_DATA += \
	client/libeventc/vapi/libeventc.deps


libeventc_la_SOURCES = \
	client/libeventc/src/libeventc.c

libeventc_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"libeventc\" \
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


libeventc_connection_test_SOURCES = \
	client/libeventc/tests/integration/connection.c

libeventc_connection_test_CFLAGS = \
	$(AM_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

libeventc_connection_test_LDADD = \
	libeventd-test.la \
	$(GLIB_LIBS)
