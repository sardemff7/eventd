#
# Internal helper lib
#
pkglib_LTLIBRARIES += \
	libeventd.la

pkginclude_HEADERS += \
	include/libeventd-regex.h \
	include/libeventd-config.h \
	include/libeventd-plugins.h

EXTRA_DIST += \
	src/libeventd/libeventd.sym

libeventd_la_SOURCES = \
	src/libeventd/regex.c \
	src/libeventd/config.c \
	src/libeventd/plugins.c

libeventd_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"libeventd\" \
	-D LIBDIR=\"$(libdir)\" \
	-D DATADIR=\"$(datadir)\" \
	$(GMODULE_CFLAGS) \
	$(GLIB_CFLAGS)

libeventd_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version \
	-export-symbols $(top_srcdir)/src/libeventd/libeventd.sym

libeventd_la_LIBADD = \
	libeventd-event.la \
	$(GMODULE_LIBS) \
	$(GLIB_LIBS)
