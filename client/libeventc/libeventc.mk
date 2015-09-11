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
	client/libeventc/include/libeventc.h \
	client/libeventc/src/libeventc.c \
	$(null)

libeventc_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-D G_LOG_DOMAIN=\"libeventc\" \
	$(GIO_CPPFLAGS) \
	$(GOBJECT_CPPFLAGS) \
	$(GLIB_CPPFLAGS) \
	$(null)

libeventc_la_CFLAGS = \
	$(AM_CFLAGS) \
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

Eventc-0.gir: EventdEvent-0.gir libeventc.la
Eventc_0_gir_INCLUDES = GObject-2.0 Gio-2.0 EventdEvent-0
Eventc_0_gir_CFLAGS = $(AM_CPPFLAGS) $(libeventc_la_CFLAGS) $(CPPFLAGS) $(CFLAGS)
Eventc_0_gir_LIBS = libeventc.la
Eventc_0_gir_FILES = $(filter-out %-private.h,$(libeventc_la_SOURCES))
INTROSPECTION_GIRS += Eventc-0.gir
