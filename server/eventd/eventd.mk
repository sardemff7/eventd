# Server

bin_PROGRAMS += \
	eventd \
	$(null)

man1_MANS += \
	server/eventd/man/eventd.1 \
	$(null)

man5_MANS += \
	server/eventd/man/eventd.conf.5 \
	server/eventd/man/eventd.event.5 \
	$(null)

noarch_pkgconfig_DATA += \
	server/eventd/pkgconfig/eventd.pc \
	$(null)

include server/eventd/events/events.mk


eventd_SOURCES = \
	src/glib-compat.h \
	server/eventd/src/types.h \
	server/eventd/src/config.h \
	server/eventd/src/config.c \
	server/eventd/src/plugins.h \
	server/eventd/src/plugins.c \
	server/eventd/src/queue.h \
	server/eventd/src/queue.c \
	server/eventd/src/control.h \
	server/eventd/src/control.c \
	server/eventd/src/sockets.h \
	server/eventd/src/sockets.c \
	server/eventd/src/eventd.h \
	server/eventd/src/eventd.c \
	$(null)

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
	$(GLIB_CFLAGS) \
	$(null)

eventd_LDADD = \
	libeventd-event.la \
	libeventd.la \
	$(SYSTEMD_LIBS) \
	$(AVAHI_LIBS) \
	$(GTHREAD_LIBS) \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GMODULE_LIBS) \
	$(GLIB_LIBS) \
	$(null)


if ENABLE_SYSTEMD

systemduserunit_DATA += \
	server/eventd/units/eventd-control.socket \
	server/eventd/units/eventd.service \
	$(null)

endif
