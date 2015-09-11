# Internal helper library

AM_CPPFLAGS += \
	-I $(srcdir)/server/libeventd/include \
	$(null)


pkglib_LTLIBRARIES += \
	libeventd.la \
	$(null)

pkginclude_HEADERS += \
	server/libeventd/include/libeventd-reconnect.h \
	server/libeventd/include/libeventd-config.h \
	$(null)


libeventd_la_SOURCES = \
	server/libeventd/include/libeventd-reconnect.h \
	server/libeventd/src/reconnect.c \
	server/libeventd/include/libeventd-config.h \
	server/libeventd/src/config.c \
	$(null)

libeventd_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-libeventd\" \
	$(NKUTILS_CPPFLAGS) \
	$(GOBJECT_CPPFLAGS) \
	$(GLIB_CPPFLAGS) \
	$(null)

libeventd_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(NKUTILS_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

libeventd_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version \
	$(null)

libeventd_la_LIBADD = \
	libeventd-event.la \
	$(NKUTILS_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)
