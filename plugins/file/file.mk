# file plugin

plugins_LTLIBRARIES += \
	file.la \
	$(null)

man5_MANS += \
	%D%/man/eventd-file.conf.5 \
	$(null)


file_la_SOURCES = \
	%D%/src/file.c \
	$(null)

file_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

file_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(PLUGIN_LDFLAGS) \
	$(null)

file_la_LIBADD = \
	libeventd.la \
	libeventd-plugin.la \
	libeventd-helpers.la \
	$(GIO_LIBS) \
	$(GLIB_LIBS) \
	$(null)
