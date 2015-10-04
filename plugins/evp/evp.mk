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
	%D%/man/eventd-evp.1 \
	$(null)

man5_MANS += \
	%D%/man/eventd-evp.conf.5 \
	$(null)


evp_la_SOURCES = \
	%D%/src/evp.h \
	%D%/src/evp.c \
	%D%/src/client.h \
	%D%/src/client.c \
	%D%/src/avahi.h \
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
	%D%/src/avahi.c \
	$(null)

endif
