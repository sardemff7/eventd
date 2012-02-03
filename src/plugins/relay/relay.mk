# relay plugin
plugins_LTLIBRARIES += \
	relay.la

relay_la_SOURCES = \
	src/plugins/relay/types.h \
	src/plugins/relay/server.h \
	src/plugins/relay/server.c \
	src/plugins/relay/relay.c

relay_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"relay\" \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS)

relay_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module -export-symbols-regex eventd_plugin_get_info

relay_la_LIBADD = \
	libeventd-event.la \
	libeventd.la \
	libeventc.la \
	$(GIO_LIBS) \
	$(GLIB_LIBS)
