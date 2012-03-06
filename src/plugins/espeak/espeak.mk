# espeak plugin
plugins_LTLIBRARIES += \
	espeak.la

espeak_la_SOURCES = \
	src/plugins/espeak/espeak.c

espeak_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-espeak\" \
	$(ESPEAK_CFLAGS) \
	$(GLIB_CFLAGS)

espeak_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module -export-symbols-regex eventd_plugin_get_info

espeak_la_LIBADD = \
	libeventd-event.la \
	libeventd.la \
	$(ESPEAK_LIBS) \
	$(GLIB_LIBS)
