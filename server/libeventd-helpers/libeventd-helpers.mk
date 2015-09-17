# Internal helper library

AM_CPPFLAGS += \
	-I $(srcdir)/server/libeventd-helpers/include \
	$(null)


pkglib_LTLIBRARIES += \
	libeventd-helpers.la \
	$(null)

pkginclude_HEADERS += \
	server/libeventd-helpers/include/libeventd-helpers-reconnect.h \
	server/libeventd-helpers/include/libeventd-helpers-config.h \
	$(null)


libeventd_helpers_la_SOURCES = \
	server/libeventd-helpers/include/libeventd-helpers-reconnect.h \
	server/libeventd-helpers/src/reconnect.c \
	server/libeventd-helpers/include/libeventd-helpers-config.h \
	server/libeventd-helpers/src/config.c \
	$(null)

libeventd_helpers_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-libeventd\" \
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
