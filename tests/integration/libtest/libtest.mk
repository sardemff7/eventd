check_LTLIBRARIES += \
	tests/integration/libeventd-test.la

tests_integration_libeventd_test_la_SOURCES = \
	tests/integration/libtest/libtest.c

tests_integration_libeventd_test_la_CFLAGS = \
	$(AM_TESTS_INTEGRATION_CFLAGS) \
	-D BUILD_DIR=\"$(builddir)\" \
	-D EXEEXT=\"$(EXEEXT)\" \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

tests_integration_libeventd_test_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-shared \
	-export-dynamic

tests_integration_libeventd_test_la_LIBADD = \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)
