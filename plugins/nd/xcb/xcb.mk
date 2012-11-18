# XCB backend

ndbackends_LTLIBRARIES += \
	xcb.la

man5_MANS += \
	plugins/nd/xcb/man/eventd-nd-xcb.conf.5


xcb_la_SOURCES = \
	plugins/nd/xcb/src/xcb.c

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
