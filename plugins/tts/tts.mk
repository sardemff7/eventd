# tts plugin

if ENABLE_TTS
plugins_LTLIBRARIES += \
	tts.la \
	$(null)

man5_MANS += \
	plugins/tts/man/eventd-tts.event.5 \
	$(null)
endif


tts_la_SOURCES = \
	plugins/tts/src/tts.c \
	$(null)

tts_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-tts\" \
	$(ESPEAK_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

tts_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module \
	$(null)

tts_la_LIBADD = \
	libeventd-event.la \
	libeventd-plugin.la \
	libeventd.la \
	$(ESPEAK_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)
