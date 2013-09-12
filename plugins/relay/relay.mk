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

man1_MANS += \
	plugins/relay/man/eventdctl-relay.1 \
	$(null)

man5_MANS += \
	plugins/relay/man/eventd-relay.event.5 \
	$(null)


relay_la_SOURCES = \
	plugins/relay/src/avahi.h \
	plugins/relay/src/server.h \
	plugins/relay/src/server.c \
	plugins/relay/src/relay.c \
	$(null)

relay_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-relay\" \
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
	libeventd-event.la \
	libeventd-evp.la \
	libeventd-plugin.la \
	libeventd.la \
	$(AVAHI_LIBS) \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)


relay_connection_test_SOURCES = \
	plugins/relay/tests/integration/connection.c \
	$(null)

relay_connection_test_CFLAGS = \
	$(AM_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

relay_connection_test_LDADD = \
	libeventd-test.la \
	libeventd-event.la \
	libeventc.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)


if ENABLE_AVAHI

relay_la_SOURCES += \
	plugins/relay/src/avahi.c \
	$(null)

endif
