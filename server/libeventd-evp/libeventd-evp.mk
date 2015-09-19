# EvP implementation library

AM_CPPFLAGS += \
	-I $(srcdir)/%D%/include \
	$(null)


noinst_LTLIBRARIES += \
	libeventd-evp.la \
	$(null)


libeventd_evp_la_SOURCES = \
	%D%/src/send.c \
	%D%/src/receive.c \
	%D%/src/context.c \
	%D%/src/context.h \
	%D%/src/helpers.c \
	%D%/include/libeventd-evp.h \
	$(null)

libeventd_evp_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

libeventd_evp_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version \
	$(null)

libeventd_evp_la_LIBADD = \
	libeventd.la \
	$(GIO_LIBS) \
	$(GLIB_LIBS) \
	$(null)
