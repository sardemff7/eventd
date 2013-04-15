# Testing helper library

AM_CFLAGS += \
	-I $(srcdir)/server/libeventd-test/include

AM_VALAFLAGS += \
	--vapidir $(srcdir)/server/libeventd-test/vapi


check_LTLIBRARIES += \
	libeventd-test.la


libeventd_test_la_SOURCES = \
	server/libeventd-test/src/libeventc-test.c \
	server/libeventd-test/src/libtest.c \
	server/libeventd-test/include/libeventd-test.h

libeventd_test_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D SRC_DIR=\"$(abs_srcdir)\" \
	-D BUILD_DIR=\"$(abs_builddir)\" \
	-D EXEEXT=\"$(EXEEXT)\" \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

libeventd_test_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-shared \
	-export-dynamic

libeventd_test_la_LIBADD = \
	libeventd-event.la \
	libeventc.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)
