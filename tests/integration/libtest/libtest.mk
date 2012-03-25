check_LTLIBRARIES += \
	tests/integration/libeventd-test.la

tests_integration_libeventd_test_la_SOURCES = \
	tests/integration/libtest/libtest.vala

tests_integration_libeventd_test_la_VALAFLAGS = \
	$(AM_TESTS_INTEGRATION_VALAFLAGS) \
	--pkg tests \
	--library libeventd-test \
	--vapi tests/integration/libeventd-test.vapi \
	--header tests/integration/libeventd-test.h \
	$(GIO_VALAFLAGS) \
	$(GOBJECT_VALAFLAGS) \
	$(GLIB_VALAFLAGS)

tests_integration_libeventd_test_la_CFLAGS = \
	$(AM_TESTS_INTEGRATION_CFLAGS) \
	-D EVENTD_TESTS_BUILD_DIR=\"$(builddir)\" \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

tests_integration_libeventd_test_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-shared \
	-export-dynamic

tests_integration_libeventd_test_la_LIBADD = \
	libeventd-event.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)
