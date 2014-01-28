# Internal helper library

AM_CPPFLAGS += \
	-I $(srcdir)/server/libeventd/include \
	$(null)


pkglib_LTLIBRARIES += \
	libeventd.la \
	$(null)

pkginclude_HEADERS += \
	server/libeventd/include/libeventd-reconnect.h \
	server/libeventd/include/libeventd-regex.h \
	server/libeventd/include/libeventd-config.h \
	$(null)


libeventd_la_SOURCES = \
	server/libeventd/src/reconnect.c \
	server/libeventd/src/regex.c \
	server/libeventd/src/config.c \
	$(null)

libeventd_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-libeventd\" \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

libeventd_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version \
	$(null)

libeventd_la_LIBADD = \
	libeventd-event.la \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)
