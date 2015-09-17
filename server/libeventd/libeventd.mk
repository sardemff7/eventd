# Main eventd library

LIBEVENTD_CURRENT=0
LIBEVENTD_REVISION=0
LIBEVENTD_AGE=0


AM_CPPFLAGS += \
	-I $(srcdir)/server/libeventd/include \
	$(null)


lib_LTLIBRARIES += \
	libeventd.la \
	$(null)

pkginclude_HEADERS += \
	server/libeventd/include/libeventd-event.h \
	$(null)

TESTS += \
	libeventd-event.test \
	$(null)

check_PROGRAMS += \
	libeventd-event.test \
	$(null)

pkgconfig_DATA += \
	server/libeventd/pkgconfig/libeventd.pc \
	$(null)

dist_vapi_DATA += \
	server/libeventd/vapi/libeventd.vapi \
	$(null)

vapi_DATA += \
	server/libeventd/vapi/libeventd.deps \
	$(null)


libeventd_la_SOURCES = \
	server/libeventd/include/libeventd-event.h \
	server/libeventd/src/event.c \
	$(null)

libeventd_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"libeventd-event\" \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

libeventd_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-version-info $(LIBEVENTD_CURRENT):$(LIBEVENTD_REVISION):$(LIBEVENTD_AGE) \
	$(null)

libeventd_la_LIBADD = \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)


libeventd_event_test_SOURCES = \
	server/libeventd/tests/unit/getters.c \
	server/libeventd/tests/unit/getters.h \
	server/libeventd/tests/unit/setters.c \
	server/libeventd/tests/unit/setters.h \
	server/libeventd/tests/unit/common.h \
	server/libeventd/tests/unit/libeventd-event.c \
	$(null)

libeventd_event_test_CFLAGS = \
	$(AM_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

libeventd_event_test_LDADD = \
	libeventd.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)

Eventd-0.gir: libeventd.la
Eventd_0_gir_INCLUDES = GObject-2.0
Eventd_0_gir_CFLAGS = $(AM_CPPFLAGS) $(libeventd_la_CFLAGS) $(CPPFLAGS) $(CFLAGS)
Eventd_0_gir_LIBS = libeventd.la
Eventd_0_gir_FILES = $(filter-out %-private.h,$(libeventd_la_SOURCES))
INTROSPECTION_GIRS += Eventd-0.gir
