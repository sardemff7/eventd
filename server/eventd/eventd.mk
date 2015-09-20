# Server

bin_PROGRAMS += \
	eventd \
	$(null)

dist_man1_MANS += \
	%D%/man/eventd.1 \
	$(null)

dist_man5_MANS += \
	%D%/man/eventd.conf.5 \
	%D%/man/eventd.event.5 \
	$(null)

noarch_pkgconfig_DATA += \
	%D%/pkgconfig/eventd.pc \
	$(null)

include %D%/events/events.mk


eventd_SOURCES = \
	%D%/src/types.h \
	%D%/src/config.h \
	%D%/src/config.c \
	%D%/src/plugins.h \
	%D%/src/plugins.c \
	%D%/src/queue.h \
	%D%/src/queue.c \
	%D%/src/control.h \
	%D%/src/control.c \
	%D%/src/sockets.h \
	%D%/src/sockets.c \
	%D%/src/eventd.h \
	%D%/src/eventd.c \
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
	libeventd.la \
	libeventd-helpers.la \
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
	%D%/units/eventd-control.socket \
	%D%/units/eventd.service \
	$(null)

endif
