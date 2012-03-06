# Linux framebuffer backend
notificationbackends_LTLIBRARIES += \
	linux.la

linux_la_SOURCES = \
	src/plugins/nd/daemon/backends/linux.c

linux_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"linux-backend\" \
	$(CAIRO_CFLAGS) \
	$(GLIB_CFLAGS)

linux_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module -export-symbols-regex eventd_nd_backend_init

linux_la_LIBADD = \
	$(CAIRO_LIBS) \
	$(GLIB_LIBS)
