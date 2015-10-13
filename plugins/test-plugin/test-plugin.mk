# test plugin

check_LTLIBRARIES += \
	test-plugin.la \
	$(null)

check_DATA += \
	$(builddir)/%D%/config/eventd.conf \
	$(builddir)/%D%/config/test.pem \
	$(builddir)/%D%/config/test.event \
	$(builddir)/%D%/config/test.action \
	$(null)

EXTRA_DIST += \
	%D%/config/eventd.conf.in \
	%D%/config/test.pem.in \
	%D%/config/test.event.in \
	%D%/config/test.action.in \
	$(null)

CLEANFILES += \
	$(builddir)/%D%/config/eventd.conf \
	$(builddir)/%D%/config/test.pem \
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

$(builddir)/%D%/config/eventd.conf: $(srcdir)/%D%/config/eventd.conf.in Makefile
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && \
		$(SED) \
		-e 's:[@]CONFIG_DIR[@]:$(abs_builddir)/%D%/config:g' \
		< $< > $@ || rm $@

$(builddir)/%D%/config/%: $(srcdir)/%D%/config/%.in
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && \
		cp $< $@
