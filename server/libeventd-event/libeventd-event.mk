# Event manipulation library

LIBEVENTD_EVENT_CURRENT=0
LIBEVENTD_EVENT_REVISION=0
LIBEVENTD_EVENT_AGE=0


AM_CPPFLAGS += \
	-I $(srcdir)/server/libeventd-event/include \
	$(null)


lib_LTLIBRARIES += \
	libeventd-event.la \
	$(null)

pkginclude_HEADERS += \
	server/libeventd-event/include/libeventd-event-types.h \
	server/libeventd-event/include/libeventd-event.h \
	$(null)

TESTS += \
	libeventd-event.test \
	$(null)

check_PROGRAMS += \
	libeventd-event.test \
	$(null)

pkgconfig_DATA += \
	server/libeventd-event/pkgconfig/libeventd-event.pc \
	$(null)

dist_vapi_DATA += \
	server/libeventd-event/vapi/libeventd-event.vapi \
	$(null)

vapi_DATA += \
	server/libeventd-event/vapi/libeventd-event.deps \
	$(null)


libeventd_event_la_SOURCES = \
	server/libeventd-event/include/libeventd-event-types.h \
	server/libeventd-event/include/libeventd-event.h \
	server/libeventd-event/src/event.c \
	$(null)

libeventd_event_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-D G_LOG_DOMAIN=\"libeventd-event\" \
	$(GOBJECT_CPPFLAGS) \
	$(GLIB_CPPFLAGS) \
	$(null)

libeventd_event_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

libeventd_event_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-version-info $(LIBEVENTD_EVENT_CURRENT):$(LIBEVENTD_EVENT_REVISION):$(LIBEVENTD_EVENT_AGE) \
	$(null)

libeventd_event_la_LIBADD = \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)


libeventd_event_test_SOURCES = \
	server/libeventd-event/tests/unit/getters.c \
	server/libeventd-event/tests/unit/getters.h \
	server/libeventd-event/tests/unit/setters.c \
	server/libeventd-event/tests/unit/setters.h \
	server/libeventd-event/tests/unit/common.h \
	server/libeventd-event/tests/unit/libeventd-event.c \
	$(null)

libeventd_event_test_CFLAGS = \
	$(AM_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

libeventd_event_test_LDADD = \
	libeventd-event.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)

EventdEvent-0.gir: libeventd-event.la
EventdEvent_0_gir_INCLUDES = GObject-2.0
EventdEvent_0_gir_CFLAGS = $(AM_CPPFLAGS) $(libeventd_event_la_CFLAGS) $(CPPFLAGS) $(CFLAGS)
EventdEvent_0_gir_SCANNERFLAGS = --identifier-prefix=Eventd
EventdEvent_0_gir_LIBS = libeventd-event.la
EventdEvent_0_gir_FILES = $(filter-out %-private.h,$(libeventd_event_la_SOURCES))
INTROSPECTION_GIRS += EventdEvent-0.gir
