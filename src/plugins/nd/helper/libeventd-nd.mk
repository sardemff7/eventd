# Helper library for nd plugin
pkglib_LTLIBRARIES += \
	libeventd-nd.la

pkginclude_HEADERS += \
	include/eventd-nd-style.h \
	include/eventd-nd-notification.h

EXTRA_DIST += \
	src/plugins/nd/helper/libeventd-nd.sym

libeventd_nd_la_SOURCES = \
	src/plugins/nd/helper/style.c \
	src/plugins/nd/helper/notification.c

libeventd_nd_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"libeventd-nd\" \
	-D SYSCONFDIR=\"$(sysconfdir)\" \
	-D LIBDIR=\"$(libdir)\" \
	-D DATADIR=\"$(datadir)\" \
	$(GLIB_CFLAGS)

libeventd_nd_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version \
	-export-symbols $(top_srcdir)/src/plugins/nd/helper/libeventd-nd.sym

libeventd_nd_la_LIBADD = \
	libeventd.la \
	$(GLIB_LIBS)
