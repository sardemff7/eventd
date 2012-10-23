# test plugin

check_LTLIBRARIES += \
	test-plugin.la

EXTRA_DIST += \
	plugins/test-plugin/events/test.event

test_plugin_la_SOURCES = \
	plugins/test-plugin/src/test-plugin.c

test_plugin_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-test-plugin\" \
	$(GLIB_CFLAGS)

test_plugin_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-rpath $(abs_builddir) \
	-avoid-version -module

test_plugin_la_LIBADD = \
	libeventd-event.la \
	$(GLIB_LIBS)
