AM_TESTS_INTEGRATION_CFLAGS = \
	$(AM_CFLAGS) \
	$(AM_VALA_CFLAGS) \
	-I $(srcdir)/tests/integration/ \
	-I $(builddir)/tests/integration/

AM_TESTS_INTEGRATION_VALAFLAGS = \
	$(AM_VALAFLAGS) \
	--vapidir $(srcdir)/tests/integration/ \
	--vapidir $(builddir)/tests/integration/

include tests/integration/libtest/libtest.mk
include tests/integration/plugin/test-plugin.mk
include tests/integration/evp/evp.mk
