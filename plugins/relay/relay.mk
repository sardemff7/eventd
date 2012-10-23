# relay plugin

plugins_LTLIBRARIES += \
	relay.la

TESTS += \
	relay-connection.test

check_PROGRAMS += \
	relay-connection.test


relay_la_SOURCES = \
	plugins/relay/src/types.h \
	plugins/relay/src/avahi.h \
	plugins/relay/src/server.h \
	plugins/relay/src/server.c \
	plugins/relay/src/relay.c

relay_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-relay\" \
	$(AVAHI_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

relay_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module

relay_la_LIBADD = \
	libeventd-event.la \
	libeventd-evp.la \
	libeventd.la \
	$(AVAHI_LIBS) \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)


relay_connection_test_SOURCES = \
	plugins/relay/tests/integration/connection.vala

relay_connection_test_VALAFLAGS = \
	$(AM_VALAFLAGS) \
	--pkg libeventd-event \
	--pkg libeventd-test \
	--pkg libeventc \
	$(GIO_VALAFLAGS) \
	$(GOBJECT_VALAFLAGS) \
	$(GLIB_VALAFLAGS)

relay_connection_test_CFLAGS = \
	$(AM_CFLAGS) \
	$(AM_VALA_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

relay_connection_test_LDADD = \
	libeventd-test.la \
	libeventd-event.la \
	libeventc.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)


if ENABLE_AVAHI

relay_la_SOURCES += \
	plugins/relay/src/avahi.c

else

relay_la_SOURCES += \
	plugins/relay/src/avahi-dummy.c

endif
