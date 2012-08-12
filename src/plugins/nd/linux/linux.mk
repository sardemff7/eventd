# Linux framebuffer backend
ndbackends_LTLIBRARIES += \
	linux.la

linux_la_SOURCES = \
	src/plugins/nd/linux/linux.c

linux_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-nd-linux-backend\" \
	$(CAIRO_CFLAGS) \
	$(GLIB_CFLAGS)

linux_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module

linux_la_LIBADD = \
	$(CAIRO_LIBS) \
	$(GLIB_LIBS)
