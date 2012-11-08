# espeak plugin

XSLTPROC_CONDITIONS += enable_espeak


plugins_LTLIBRARIES += \
	espeak.la

man5_MANS += \
	plugins/espeak/man/eventd-espeak.event.5


espeak_la_SOURCES = \
	plugins/espeak/src/espeak.c

espeak_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-espeak\" \
	$(ESPEAK_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

espeak_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module

espeak_la_LIBADD = \
	libeventd-event.la \
	libeventd.la \
	$(ESPEAK_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)
