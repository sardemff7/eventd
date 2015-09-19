# EvP implementation library

AM_CPPFLAGS += \
	-I $(srcdir)/server/libeventd-evp/include \
	$(null)


noinst_LTLIBRARIES += \
	libeventd-evp.la \
	$(null)


libeventd_evp_la_SOURCES = \
	server/libeventd-evp/src/send.c \
	server/libeventd-evp/src/receive.c \
	server/libeventd-evp/src/context.c \
	server/libeventd-evp/src/context.h \
	server/libeventd-evp/src/helpers.c \
	server/libeventd-evp/include/libeventd-evp.h \
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
