# test plugin

check_LTLIBRARIES += \
	test-plugin.la \
	$(null)

dist_check_DATA += \
	$(builddir)/%D%/config/eventd.conf \
	$(builddir)/%D%/config/test.event \
	$(builddir)/%D%/config/test.action \
	$(null)


test_plugin_la_SOURCES = \
	server/libeventd/include/libeventd-event-private.h \
	server/libeventd/src/event-private.c \
	%D%/src/test-plugin.c \
	$(null)

test_plugin_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(NKUTILS_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

test_plugin_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-rpath $(abs_builddir) \
	$(PLUGIN_LDFLAGS) \
	$(null)

test_plugin_la_LIBADD = \
	libeventd.la \
	libeventd-plugin.la \
	$(NKUTILS_LIBS) \
	$(GLIB_LIBS) \
	$(null)
