# Server
bin_PROGRAMS += \
	eventd

pkginclude_HEADERS += \
	include/eventd-core-interface.h \
	include/eventd-plugin.h

man1_MANS += \
	man/eventd.1

man5_MANS += \
	man/eventd.conf.5 \
	man/eventd.event.5

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
	src/eventd/eventd.h \
	src/eventd/eventd.c

eventd_CFLAGS = \
	$(AM_CFLAGS) \
	-D SYSCONFDIR=\"$(sysconfdir)\" \
	-D LIBDIR=\"$(libdir)\" \
	-D DATADIR=\"$(datadir)\" \
	$(SYSTEMD_CFLAGS) \
	$(AVAHI_CFLAGS) \
	$(GTHREAD_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GMODULE_CFLAGS) \
	$(GLIB_CFLAGS)

eventd_LDADD = \
	libeventd-event.la \
	libeventd.la \
	$(SYSTEMD_LIBS) \
	$(AVAHI_LIBS) \
	$(GTHREAD_LIBS) \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GMODULE_LIBS) \
	$(GLIB_LIBS)


if ENABLE_SYSTEMD

systemduserunit_DATA += \
	data/units/eventd-evp.socket \
	data/units/eventd-control.socket \
	data/units/eventd.service

endif
