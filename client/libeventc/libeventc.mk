# Client library

LIBEVENTC_CURRENT=0
LIBEVENTC_REVISION=0
LIBEVENTC_AGE=0


AM_CFLAGS += \
	-I $(srcdir)/%D%/include \
	$(null)


lib_LTLIBRARIES += \
	libeventc.la \
	$(null)

pkginclude_HEADERS += \
	%D%/include/libeventc.h \
	$(null)

TESTS += \
	libeventc-connection.test \
	$(null)

check_PROGRAMS += \
	libeventc-connection.test \
	$(null)

pkgconfig_DATA += \
	%D%/pkgconfig/libeventc.pc \
	$(null)

dist_vapi_DATA += \
	%D%/vapi/libeventc.vapi \
	$(null)

vapi_DATA += \
	%D%/vapi/libeventc.deps \
	$(null)


libeventc_la_SOURCES = \
	%D%/src/libeventc.c \
	server/modules/src/ws-load.c \
	$(null)

libeventc_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GMODULE_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

libeventc_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-shared \
	-version-info $(LIBEVENTC_CURRENT):$(LIBEVENTC_REVISION):$(LIBEVENTC_AGE) \
	$(null)

libeventc_la_LIBADD = \
	libeventd.la \
	$(GIO_LIBS) \
	$(GMODULE_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)


libeventc_connection_test_SOURCES = \
	%D%/tests/integration/connection.c \
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

Eventc-0.gir: Eventd-0.gir libeventc.la
Eventc_0_gir_INCLUDES = GObject-2.0 Gio-2.0 Eventd-0
Eventc_0_gir_CFLAGS = $(AM_CFLAGS) $(libeventc_la_CFLAGS) $(CFLAGS)
Eventc_0_gir_LIBS = libeventc.la
Eventc_0_gir_FILES = $(filter-out %.h,$(libeventc_la_SOURCES)) $(filter %D%/include/%.h,$(pkginclude_HEADERS))
INTROSPECTION_GIRS += Eventc-0.gir
