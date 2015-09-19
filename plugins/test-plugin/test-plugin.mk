# test plugin

check_LTLIBRARIES += \
	test-plugin.la \
	$(null)

EXTRA_DIST += \
	plugins/test-plugin/events/test.event \
	$(null)

test_plugin_la_SOURCES = \
	plugins/test-plugin/src/test-plugin.c \
	$(null)

test_plugin_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

test_plugin_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-rpath $(abs_builddir) \
	-avoid-version -module \
	$(null)

test_plugin_la_LIBADD = \
	libeventd.la \
	libeventd-plugin.la \
	$(GLIB_LIBS) \
	$(null)
