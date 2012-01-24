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

EXTRA_DIST += \
	src/libeventd-event/libeventd-event.sym

libeventd_event_la_SOURCES = \
	src/libeventd-event/event.c

libeventd_event_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"libeventd-event\" \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

libeventd_event_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-version-info $(LIBEVENTD_EVENT_CURRENT):$(LIBEVENTD_EVENT_REVISION):$(LIBEVENTD_EVENT_AGE) \
	-Wl,--version-script=$(top_srcdir)/src/libeventd-event/libeventd-event.sym

libeventd_event_la_LIBADD = \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)

pkgconfig_DATA += \
	$(top_builddir)/data/libeventd-event.pc

dist_vapi_DATA += \
	$(top_builddir)/vapi/libeventd-event.deps \
	vapi/libeventd-event.vapi
