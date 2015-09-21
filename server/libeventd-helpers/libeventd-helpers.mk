# Internal helper library

AM_CFLAGS += \
	-I $(srcdir)/%D%/include \
	$(null)


pkglib_LTLIBRARIES += \
	libeventd-helpers.la \
	$(null)

pkginclude_HEADERS += \
	%D%/include/libeventd-helpers-reconnect.h \
	%D%/include/libeventd-helpers-config.h \
	$(null)


libeventd_helpers_la_SOURCES = \
	%D%/include/libeventd-helpers-reconnect.h \
	%D%/src/reconnect.c \
	%D%/include/libeventd-helpers-config.h \
	%D%/src/config.c \
	$(null)

libeventd_helpers_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(NKUTILS_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

libeventd_helpers_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version \
	$(null)

libeventd_helpers_la_LIBADD = \
	libeventd.la \
	$(NKUTILS_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)
