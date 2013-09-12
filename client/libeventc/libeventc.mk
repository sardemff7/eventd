# Client library

LIBEVENTC_CURRENT=0
LIBEVENTC_REVISION=0
LIBEVENTC_AGE=0


AM_CPPFLAGS += \
	-I $(srcdir)/client/libeventc/include \
	$(null)


lib_LTLIBRARIES += \
	libeventc.la \
	$(null)

pkginclude_HEADERS += \
	client/libeventc/include/libeventc.h \
	$(null)

TESTS += \
	libeventc-connection.test \
	$(null)

check_PROGRAMS += \
	libeventc-connection.test \
	$(null)

pkgconfig_DATA += \
	client/libeventc/pkgconfig/libeventc.pc \
	$(null)

dist_vapi_DATA += \
	client/libeventc/vapi/libeventc.vapi \
	$(null)

vapi_DATA += \
	client/libeventc/vapi/libeventc.deps \
	$(null)


libeventc_la_SOURCES = \
	client/libeventc/src/libeventc.c \
	$(null)

libeventc_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"libeventc\" \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

libeventc_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-shared \
	-version-info $(LIBEVENTC_CURRENT):$(LIBEVENTC_REVISION):$(LIBEVENTC_AGE) \
	$(null)

libeventc_la_LIBADD = \
	libeventd-event.la \
	libeventd-evp.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)


libeventc_connection_test_SOURCES = \
	client/libeventc/tests/integration/connection.c \
	$(null)

libeventc_connection_test_CFLAGS = \
	$(AM_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

libeventc_connection_test_LDADD = \
	libeventd-test.la \
	$(GLIB_LIBS) \
	$(null)
