#
# Internal helper lib
#
pkglib_LTLIBRARIES += \
	libeventd.la

pkginclude_HEADERS += \
	include/libeventd-regex.h \
	include/libeventd-config.h

libeventd_la_SOURCES = \
	src/libeventd/regex.c \
	src/libeventd/config.c

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
	include/libeventd-nd-notification-template.h \
	include/libeventd-nd-notification.h \
	include/libeventd-nd-notification-types.h

libeventd_la_SOURCES += \
	src/libeventd/nd-notification.c

libeventd_la_CFLAGS += \
	$(GDK_PIXBUF_CFLAGS)

libeventd_la_LIBADD += \
	$(GDK_PIXBUF_LIBS)
endif
