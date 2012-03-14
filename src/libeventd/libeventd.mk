#
# Internal helper lib
#
pkglib_LTLIBRARIES += \
	libeventd.la

pkginclude_HEADERS += \
	include/libeventd-regex.h \
	include/libeventd-config.h

EXTRA_DIST += \
	src/libeventd/libeventd.sym

libeventd_la_SOURCES = \
	src/libeventd/regex.c \
	src/libeventd/config.c

libeventd_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-libeventd\" \
	$(GMODULE_CFLAGS) \
	$(GLIB_CFLAGS)

libeventd_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version \
	-export-symbols $(srcdir)/src/libeventd/libeventd.sym

libeventd_la_LIBADD = \
	libeventd-event.la \
	$(GMODULE_LIBS) \
	$(GLIB_LIBS)
