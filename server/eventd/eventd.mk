# Server

bin_PROGRAMS += \
	eventd \
	$(null)

man1_MANS += \
	%D%/man/eventd.1 \
	$(null)

man5_MANS += \
	%D%/man/eventd.conf.5 \
	$(null)

noarch_pkgconfig_DATA += \
	%D%/pkgconfig/eventd.pc \
	$(null)


eventd_SOURCES = \
	%D%/src/types.h \
	%D%/src/config.h \
	%D%/src/config.c \
	%D%/src/events.h \
	%D%/src/events.c \
	%D%/src/actions.h \
	%D%/src/actions.c \
	%D%/src/plugins.h \
	%D%/src/plugins.c \
	%D%/src/control.h \
	%D%/src/control.c \
	%D%/src/sockets.h \
	%D%/src/sockets.c \
	%D%/src/eventd.h \
	%D%/src/eventd.c \
	$(null)

eventd_CFLAGS = \
	$(AM_CFLAGS) \
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
	%D%/units/user/eventd-control.socket \
	%D%/units/user/eventd.socket \
	%D%/units/user/eventd.service \
	$(null)

systemdsystemunit_DATA += \
	%D%/units/system/eventd-control.socket \
	%D%/units/system/eventd.socket \
	%D%/units/system/eventd.service \
	$(null)

endif
