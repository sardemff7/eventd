check_LTLIBRARIES += \
	test-plugin.la

test_plugin_la_SOURCES = \
	tests/integration/plugin/test-plugin.c

test_plugin_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-test-plugin\" \
	$(GLIB_CFLAGS)

test_plugin_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-rpath $(abs_builddir) \
	-avoid-version -module -export-symbols-regex 'eventd_plugin_(id|get_info)'

test_plugin_la_LIBADD = \
	libeventd-event.la \
	$(GLIB_LIBS)
