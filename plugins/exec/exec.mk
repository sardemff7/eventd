# exec plugin

plugins_LTLIBRARIES += \
	exec.la

man5_MANS += \
	plugins/exec/man/eventd-exec.event.5


exec_la_SOURCES = \
	plugins/exec/src/exec.c

exec_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-exec\" \
	$(GLIB_CFLAGS)

exec_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module

exec_la_LIBADD = \
	libeventd-event.la \
	libeventd.la \
	$(GLIB_LIBS)
