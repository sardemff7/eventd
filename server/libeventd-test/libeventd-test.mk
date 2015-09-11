# Testing helper library

AM_CPPFLAGS += \
	-I $(srcdir)/server/libeventd-test/include \
	$(null)


check_LTLIBRARIES += \
	libeventd-test.la \
	$(null)


libeventd_test_la_SOURCES = \
	server/libeventd-test/src/libeventc-test.c \
	server/libeventd-test/src/libtest.c \
	server/libeventd-test/include/libeventd-test.h \
	$(null)

libeventd_test_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-D SRC_DIR=\"$(abs_srcdir)\" \
	-D BUILD_DIR=\"$(abs_builddir)\" \
	-D EXEEXT=\"$(EXEEXT)\" \
	$(GIO_CPPFLAGS) \
	$(GOBJECT_CPPFLAGS) \
	$(GLIB_CPPFLAGS) \
	$(null)

libeventd_test_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

libeventd_test_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-shared \
	-export-dynamic \
	$(null)

libeventd_test_la_LIBADD = \
	libeventd-event.la \
	libeventc.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)
