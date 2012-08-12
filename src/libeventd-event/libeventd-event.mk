#
# Event lib
#
LIBEVENTD_EVENT_CURRENT=0
LIBEVENTD_EVENT_REVISION=0
LIBEVENTD_EVENT_AGE=0

lib_LTLIBRARIES += \
	libeventd-event.la

pkginclude_HEADERS += \
	include/libeventd-event-types.h \
	include/libeventd-event.h

libeventd_event_la_SOURCES = \
	src/libeventd-event/event.c

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

pkgconfig_DATA += \
	data/libeventd-event.pc

dist_vapi_DATA += \
	vapi/libeventd-event.deps \
	vapi/libeventd-event.vapi
