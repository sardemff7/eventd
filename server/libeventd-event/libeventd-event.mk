# Event manipulation library

LIBEVENTD_EVENT_CURRENT=0
LIBEVENTD_EVENT_REVISION=0
LIBEVENTD_EVENT_AGE=0


AM_CFLAGS += \
	-I $(srcdir)/server/libeventd-event/include

AM_VALAFLAGS += \
	--vapidir $(srcdir)/server/libeventd-event/vapi


lib_LTLIBRARIES += \
	libeventd-event.la

pkginclude_HEADERS += \
	server/libeventd-event/include/libeventd-event-private.h \
	server/libeventd-event/include/libeventd-event-types.h \
	server/libeventd-event/include/libeventd-event.h

TESTS += \
	libeventd-event.test

check_PROGRAMS += \
	libeventd-event.test

pkgconfig_DATA += \
	server/libeventd-event/pkgconfig/libeventd-event.pc

dist_vapi_DATA += \
	server/libeventd-event/vapi/libeventd-event.vapi

vapi_DATA += \
	server/libeventd-event/vapi/libeventd-event.deps


libeventd_event_la_SOURCES = \
	server/libeventd-event/src/event.c

libeventd_event_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"libeventd-event\" \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

libeventd_event_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-version-info $(LIBEVENTD_EVENT_CURRENT):$(LIBEVENTD_EVENT_REVISION):$(LIBEVENTD_EVENT_AGE)

libeventd_event_la_LIBADD = \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)


libeventd_event_test_SOURCES = \
	server/libeventd-event/tests/unit/getters.c \
	server/libeventd-event/tests/unit/getters.h \
	server/libeventd-event/tests/unit/setters.c \
	server/libeventd-event/tests/unit/setters.h \
	server/libeventd-event/tests/unit/common.h \
	server/libeventd-event/tests/unit/libeventd-event.c

libeventd_event_test_CFLAGS = \
	$(AM_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

libeventd_event_test_LDADD = \
	libeventd-event.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)
