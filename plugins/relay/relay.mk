# relay plugin

plugins_LTLIBRARIES += \
	relay.la \
	$(null)

TESTS += \
	relay-connection.test \
	$(null)

check_PROGRAMS += \
	relay-connection.test \
	$(null)

dist_man1_MANS += \
	%D%/man/eventdctl-relay.1 \
	$(null)

dist_man5_MANS += \
	%D%/man/eventd-relay.event.5 \
	$(null)


relay_la_SOURCES = \
	%D%/src/avahi.h \
	%D%/src/server.h \
	%D%/src/server.c \
	%D%/src/relay.c \
	$(null)

relay_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(AVAHI_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

relay_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module \
	$(null)

relay_la_LIBADD = \
	libeventd.la \
	libeventd-evp.la \
	libeventd-plugin.la \
	libeventd-helpers.la \
	$(AVAHI_LIBS) \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)


relay_connection_test_SOURCES = \
	%D%/tests/integration/connection.c \
	$(null)

relay_connection_test_CFLAGS = \
	$(AM_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

relay_connection_test_LDADD = \
	libeventd-test.la \
	libeventd.la \
	libeventc.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)


if ENABLE_AVAHI

relay_la_SOURCES += \
	%D%/src/avahi.c \
	$(null)

endif
