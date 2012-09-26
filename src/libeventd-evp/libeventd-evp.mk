noinst_LTLIBRARIES += \
	libeventd-evp.la

libeventd_evp_la_SOURCES = \
	src/libeventd-evp/send.c \
	src/libeventd-evp/receive.c \
	src/libeventd-evp/context.c \
	src/libeventd-evp/context.h \
	src/libeventd-evp/helpers.c

libeventd_evp_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"libeventd-evp\" \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS)

libeventd_evp_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version

libeventd_evp_la_LIBADD = \
	$(GIO_LIBS) \
	$(GLIB_LIBS)
