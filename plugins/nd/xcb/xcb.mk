# XCB backend

XSLTPROC_CONDITIONS += enable_nd_xcb


ndbackends_LTLIBRARIES += \
	xcb.la


xcb_la_SOURCES = \
	plugins/nd/xcb/xcb.c

xcb_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-nd-xcb-backend\" \
	$(XCB_CFLAGS) \
	$(CAIRO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

xcb_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module

xcb_la_LIBADD = \
	libeventd-event.la \
	libeventd.la \
	$(XCB_LIBS) \
	$(CAIRO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)

if ENABLE_DBUS
XSLTPROC_CONDITIONS += enable_notification_daemon_dropin_replacement
endif
