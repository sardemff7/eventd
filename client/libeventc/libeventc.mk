# Client library

LIBEVENTC_CURRENT=0
LIBEVENTC_REVISION=0
LIBEVENTC_AGE=0


AM_CFLAGS += \
	-I $(srcdir)/client/libeventc/include

AM_VALAFLAGS += \
	--vapidir $(srcdir)/client/libeventc/vapi


lib_LTLIBRARIES += \
	libeventc.la

pkginclude_HEADERS += \
	client/libeventc/include/libeventc.h

TESTS += \
	libeventc-connection.test \
	libeventc-reconnection.test

check_PROGRAMS += \
	libeventc-connection.test \
	libeventc-reconnection.test

pkgconfig_DATA += \
	client/libeventc/pkgconfig/libeventc.pc

dist_vapi_DATA += \
	client/libeventc/vapi/libeventc.vapi

vapi_DATA += \
	client/libeventc/vapi/libeventc.deps


libeventc_la_SOURCES = \
	src/config.vapi \
	client/libeventc/src/libeventc.vala

libeventc_la_VALAFLAGS = \
	$(AM_VALAFLAGS) \
	--pkg libeventd-event \
	--pkg libeventd-event-private \
	--pkg libeventd-evp \
	--library libeventc \
	--vapi client/libeventc/vapi/libeventc.vapi \
	--header client/libeventc/include/libeventc.h \
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


libeventc_connection_test_SOURCES = \
	client/libeventc/tests/integration/connection.vala

libeventc_connection_test_VALAFLAGS = \
	$(AM_VALAFLAGS) \
	--pkg libeventd-test \
	--pkg libeventd-event \
	--pkg libeventc \
	$(GIO_VALAFLAGS) \
	$(GOBJECT_VALAFLAGS) \
	$(GLIB_VALAFLAGS)

libeventc_connection_test_CFLAGS = \
	$(AM_CFLAGS) \
	$(AM_VALA_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

libeventc_connection_test_LDADD = \
	libeventd-test.la \
	libeventd-event.la \
	libeventc.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)


libeventc_reconnection_test_SOURCES = \
	client/libeventc/tests/integration/reconnection.vala

libeventc_reconnection_test_VALAFLAGS = \
	$(AM_VALAFLAGS) \
	--pkg libeventd-test \
	--pkg libeventd-event \
	--pkg libeventc \
	$(GIO_VALAFLAGS) \
	$(GOBJECT_VALAFLAGS) \
	$(GLIB_VALAFLAGS)

libeventc_reconnection_test_CFLAGS = \
	$(AM_CFLAGS) \
	$(AM_VALA_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

libeventc_reconnection_test_LDADD = \
	libeventd-test.la \
	libeventd-event.la \
	libeventc.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)
