# test plugin

check_LTLIBRARIES += \
	test-plugin.la \
	$(null)

dist_check_DATA += \
	$(builddir)/%D%/config/test.event \
	$(builddir)/%D%/config/test.action \
	$(null)


test_plugin_la_SOURCES = \
	%D%/src/test-plugin.c \
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
