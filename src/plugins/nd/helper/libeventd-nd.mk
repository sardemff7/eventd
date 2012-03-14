# Helper library for nd plugin
pkglib_LTLIBRARIES += \
	libeventd-nd.la

pkginclude_HEADERS += \
	include/eventd-nd-style.h \
	include/eventd-nd-notification.h \
	include/eventd-nd-types.h

EXTRA_DIST += \
	src/plugins/nd/helper/libeventd-nd.sym

libeventd_nd_la_SOURCES = \
	src/plugins/nd/helper/style.c \
	src/plugins/nd/helper/notification.c

libeventd_nd_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"libeventd-nd\" \
	$(GDK_PIXBUF_CFLAGS) \
	$(GLIB_CFLAGS)

libeventd_nd_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version \
	-export-symbols $(srcdir)/src/plugins/nd/helper/libeventd-nd.sym

libeventd_nd_la_LIBADD = \
	libeventd.la \
	$(GDK_PIXBUF_LIBS) \
	$(GLIB_LIBS)
