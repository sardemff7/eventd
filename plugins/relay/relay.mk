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


relay_la_SOURCES = \
	%D%/src/dns-sd.h \
	%D%/src/dns-sd.c \
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
	$(PLUGIN_LDFLAGS) \
	$(null)

relay_la_LIBADD = \
	libeventd.la \
	libeventc.la \
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
