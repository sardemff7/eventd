# test plugin

check_LTLIBRARIES += \
	test-plugin.la \
	$(null)

check_DATA += \
	$(srcdir)/%D%/config/eventd.conf \
	$(null)

EXTRA_DIST += \
	%D%/config/eventd.conf.in \
	%D%/config/test.event \
	%D%/config/test.action \
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

$(srcdir)/%D%/config/eventd.conf: %D%/config/eventd.conf.in Makefile
	$(AM_V_GEN)$(SED) \
		-e 's:[@]CONFIG_DIR[@]:$(abs_srcdir)/%D%/config:g' \
		< $< > $@ || rm $@
