# Testing helper library

AM_CFLAGS += \
	-I $(srcdir)/%D%/include \
	$(null)


check_LTLIBRARIES += \
	libeventd-test.la \
	$(null)


libeventd_test_la_SOURCES = \
	%D%/src/libeventc-test.c \
	%D%/src/libtest.c \
	%D%/include/libeventd-test.h \
	$(null)

libeventd_test_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D SRC_DIR='"$(abs_srcdir)"' \
	-D EVENTD_PATH='"$(abs_builddir)/eventd$(EXEEXT)"' \
	-D EVENTDCTL_PATH='"$(abs_builddir)/eventdctl$(EXEEXT)"' \
	-D TEST_PLUGIN_PATH='"$(abs_builddir)/$(LT_OBJDIR)"' \
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
	libeventd.la \
	libeventc.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)
