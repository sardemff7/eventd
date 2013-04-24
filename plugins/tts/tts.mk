# tts plugin

if ENABLE_TTS
plugins_LTLIBRARIES += \
	tts.la

man5_MANS += \
	plugins/tts/man/eventd-tts.event.5
endif


tts_la_SOURCES = \
	plugins/tts/src/tts.c

tts_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-tts\" \
	$(ESPEAK_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

tts_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module

tts_la_LIBADD = \
	libeventd-event.la \
	libeventd-plugin.la \
	libeventd.la \
	$(ESPEAK_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)
