# exec plugin

plugins_LTLIBRARIES += \
	exec.la \
	$(null)

man5_MANS += \
	%D%/man/eventd-exec.conf.5 \
	$(null)


exec_la_SOURCES = \
	%D%/src/exec.c \
	$(null)

exec_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

exec_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(PLUGIN_LDFLAGS) \
	$(null)

exec_la_LIBADD = \
	libeventd.la \
	libeventd-plugin.la \
	libeventd-helpers.la \
	$(GLIB_LIBS) \
	$(null)
