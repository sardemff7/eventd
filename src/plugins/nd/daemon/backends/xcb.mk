# XCP framebuffer backend
ndbackends_LTLIBRARIES += \
	xcb.la

xcb_la_SOURCES = \
	src/plugins/nd/daemon/backends/xcb.c

xcb_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-nd-xcb-backend\" \
	$(XCB_CFLAGS) \
	$(CAIRO_CFLAGS) \
	$(GLIB_CFLAGS)

xcb_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module -export-symbols-regex eventd_nd_backend_init

xcb_la_LIBADD = \
	$(XCB_LIBS) \
	$(CAIRO_LIBS) \
	$(GLIB_LIBS)
