# EvP implementation library

AM_CFLAGS += \
	-I $(srcdir)/server/libeventd-evp/include


noinst_LTLIBRARIES += \
	libeventd-evp.la


libeventd_evp_la_SOURCES = \
	src/glib-compat.h \
	server/libeventd-evp/src/send.c \
	server/libeventd-evp/src/receive.c \
	server/libeventd-evp/src/context.c \
	server/libeventd-evp/src/context.h \
	server/libeventd-evp/src/helpers.c \
	server/libeventd-evp/include/libeventd-evp.h

libeventd_evp_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"libeventd-evp\" \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS)

libeventd_evp_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version

libeventd_evp_la_LIBADD = \
	libeventd-event.la \
	$(GIO_LIBS) \
	$(GLIB_LIBS)
