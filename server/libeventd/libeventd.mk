# Internal helper library

AM_CFLAGS += \
	-I $(srcdir)/server/libeventd/include


pkglib_LTLIBRARIES += \
	libeventd.la

pkginclude_HEADERS += \
	server/libeventd/include/libeventd-regex.h \
	server/libeventd/include/libeventd-config.h


libeventd_la_SOURCES = \
	server/libeventd/src/regex.c \
	server/libeventd/src/config.c

libeventd_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-libeventd\" \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

libeventd_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version

libeventd_la_LIBADD = \
	libeventd-event.la \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)


if ENABLE_NOTIFICATION_DAEMON

pkginclude_HEADERS += \
	server/libeventd/include/libeventd-nd-notification-template.h \
	server/libeventd/include/libeventd-nd-notification.h \
	server/libeventd/include/libeventd-nd-notification-types.h

libeventd_la_SOURCES += \
	server/libeventd/src/nd-notification.c

libeventd_la_CFLAGS += \
	$(GDK_PIXBUF_CFLAGS)

libeventd_la_LIBADD += \
	$(GDK_PIXBUF_LIBS)

endif
