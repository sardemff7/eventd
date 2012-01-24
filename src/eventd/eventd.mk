# Server
bin_PROGRAMS += \
	eventd

pkginclude_HEADERS += \
	include/eventd-plugin.h

eventd_SOURCES = \
	src/eventd/types.h \
	src/eventd/config.h \
	src/eventd/config.c \
	src/eventd/plugins.h \
	src/eventd/plugins.c \
	include/glib-compat.h \
	src/eventd/queue.h \
	src/eventd/queue.c \
	src/eventd/control.h \
	src/eventd/control.c \
	src/eventd/sockets.h \
	src/eventd/sockets.c \
	src/eventd/dbus.h \
	src/eventd/service.h \
	src/eventd/service.c \
	src/eventd/eventd.c

eventd_CFLAGS = \
	$(AM_CFLAGS) \
	-D SYSCONFDIR=\"$(sysconfdir)\" \
	-D DATADIR=\"$(datadir)\" \
	$(SYSTEMD_CFLAGS) \
	$(GTHREAD_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS)

eventd_LDADD = \
	libeventd-event.la \
	libeventd.la \
	$(SYSTEMD_LIBS) \
	$(GTHREAD_LIBS) \
	$(GIO_LIBS) \
	$(GLIB_LIBS)

if ENABLE_DBUS
eventd_SOURCES += \
	src/eventd/dbus.c
else
eventd_SOURCES += \
	src/eventd/dbus-dummy.c
endif

if ENABLE_DBUS
dist_pkgdata_DATA += \
	data/libnotify.conf
endif
