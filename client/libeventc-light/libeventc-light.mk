# Client library

LIBEVENTC_LIGHT_CURRENT=0
LIBEVENTC_LIGHT_REVISION=0
LIBEVENTC_LIGHT_AGE=0


AM_CFLAGS += \
	-I $(srcdir)/%D%/include \
	$(null)


lib_LTLIBRARIES += \
	libeventc-light.la \
	$(null)

pkginclude_HEADERS += \
	%D%/include/libeventc-light.h \
	$(null)

pkgconfig_DATA += \
	%D%/pkgconfig/libeventc-light.pc \
	$(null)


libeventc_light_la_SOURCES = \
	%D%/src/libeventc-light.c \
	$(null)

libeventc_light_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

libeventc_light_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-shared \
	-version-info $(LIBEVENTC_LIGHT_CURRENT):$(LIBEVENTC_LIGHT_REVISION):$(LIBEVENTC_LIGHT_AGE) \
	$(null)

libeventc_light_la_LIBADD = \
	libeventd.la \
	$(GLIB_LIBS) \
	$(null)
