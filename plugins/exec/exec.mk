# exec plugin

plugins_LTLIBRARIES += \
	exec.la \
	$(null)

man5_MANS += \
	plugins/exec/man/eventd-exec.event.5 \
	$(null)


exec_la_SOURCES = \
	plugins/exec/src/exec.c \
	$(null)

exec_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-exec\" \
	$(GLIB_CFLAGS) \
	$(null)

exec_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module \
	$(null)

exec_la_LIBADD = \
	libeventd.la \
	libeventd-plugin.la \
	libeventd-helpers.la \
	$(GLIB_LIBS) \
	$(null)
