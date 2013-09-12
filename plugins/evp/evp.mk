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

man1_MANS += \
	plugins/evp/man/eventd-evp.1 \
	$(null)

man5_MANS += \
	plugins/evp/man/eventd-evp.conf.5 \
	plugins/evp/man/eventd-evp.event.5 \
	$(null)


evp_la_SOURCES = \
	plugins/evp/src/evp.c \
	$(null)

evp_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-evp\" \
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
	libeventd-event.la \
	libeventd-evp.la \
	libeventd-plugin.la \
	libeventd.la \
	$(AVAHI_LIBS) \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)


evp_connection_test_SOURCES = \
	plugins/evp/tests/integration/connection.c \
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
	plugins/evp/src/avahi.h \
	plugins/evp/src/avahi.c \
	$(null)

endif


if ENABLE_SYSTEMD

systemduserunit_DATA += \
	plugins/evp/units/eventd-evp.socket \
	$(null)

endif
