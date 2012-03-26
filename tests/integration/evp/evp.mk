TESTS += \
	tests/integration/evp-connection.test

check_PROGRAMS += \
	tests/integration/evp-connection.test

tests_integration_evp_connection_test_SOURCES = \
	tests/integration/evp/connection.vala

tests_integration_evp_connection_test_VALAFLAGS = \
	$(AM_TESTS_INTEGRATION_VALAFLAGS) \
	--pkg libeventd-test \
	$(GIO_VALAFLAGS) \
	$(GOBJECT_VALAFLAGS) \
	$(GLIB_VALAFLAGS)

tests_integration_evp_connection_test_CFLAGS = \
	$(AM_TESTS_INTEGRATION_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

tests_integration_evp_connection_test_LDADD = \
	tests/integration/libeventd-test.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)

$(srcdir)/tests_integration_evp_connection_test_vala.stamp: $(srcdir)/tests_integration_libeventd_test_la_vala.stamp
