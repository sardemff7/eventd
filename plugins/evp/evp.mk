# EVENT protocol plugin

plugins_LTLIBRARIES += \
	evp.la \
	$(null)

TESTS += \
	evp-connection.test \
	$(null)

check_PROGRAMS += \
	evp-connection.test \
	$(null)

dist_man1_MANS += \
	%D%/man/eventd-evp.1 \
	$(null)

dist_man5_MANS += \
	%D%/man/eventd-evp.conf.5 \
	%D%/man/eventd-evp.event.5 \
	$(null)


evp_la_SOURCES = \
	%D%/src/evp.c \
	$(null)

evp_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(AVAHI_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

evp_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module \
	$(null)

evp_la_LIBADD = \
	libeventd.la \
	libeventd-evp.la \
	libeventd-plugin.la \
	libeventd-helpers.la \
	$(AVAHI_LIBS) \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)


evp_connection_test_SOURCES = \
	%D%/tests/integration/connection.c \
	$(null)

evp_connection_test_CFLAGS = \
	$(AM_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

evp_connection_test_LDADD = \
	libeventd-test.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)


if ENABLE_AVAHI

evp_la_SOURCES += \
	%D%/src/avahi.h \
	%D%/src/avahi.c \
	$(null)

endif


if ENABLE_SYSTEMD

systemduserunit_DATA += \
	%D%/units/eventd-evp.socket \
	$(null)

endif
