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

exec_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-exec\" \
	$(GLIB_CPPFLAGS) \
	$(null)

exec_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

exec_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module \
	$(null)

exec_la_LIBADD = \
	libeventd-event.la \
	libeventd-plugin.la \
	libeventd.la \
	$(GLIB_LIBS) \
	$(null)
